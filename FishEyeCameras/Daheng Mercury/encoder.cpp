#include "encoder.h"
#include <string.h>

int Encoder::rgb_to_i420(int width, int height, uint8_t *src, uint8_t *dst)
{
  int i, j;
  unsigned char *dst_y_even;
  unsigned char *dst_y_odd;
  unsigned char *dst_u;
  unsigned char *dst_v;
  const unsigned char *src_even;
  const unsigned char *src_odd;
  src_even = (const unsigned char *)src;
  src_odd = src_even + width * 3;

  dst_y_even = (unsigned char *)dst;
  dst_y_odd = dst_y_even + width;
  dst_u = dst_y_even + width * height;
  dst_v = dst_u + ((width * height) >> 2);

  for (i = 0; i < height / 2; ++i) {
    for (j = 0; j < width / 2; ++j) {
      short r, g, b;
      r = *src_even++;
      g = *src_even++;
      b = *src_even++;

      *dst_y_even++ = (unsigned char)
        (((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);
      *dst_u++ = (unsigned char)
        (((r * -38 - g * 74 + b * 112 + 128) >> 8) + 128);
      *dst_v++ = (unsigned char)
        (((r * 112 - g * 94 - b * 18 + 128) >> 8) + 128);

      r = *src_even++;
      g = *src_even++;
      b = *src_even++;

      *dst_y_even++ = (unsigned char)
        (((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);

      r = *src_odd++;
      g = *src_odd++;
      b = *src_odd++;

      *dst_y_odd++ = (unsigned char)
        (((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);

      r = *src_odd++;
      g = *src_odd++;
      b = *src_odd++;

      *dst_y_odd++ = (unsigned char)
        (((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);
    }

    dst_y_odd += width;
    dst_y_even += width;
    src_odd += width * 3;
    src_even += width * 3;
  }

  return 0;
}

size_t Encoder::get_i420_framesize(int width, int height)
{
  size_t size = (size_t)width * (size_t)height;
  size_t qsize = size / 4;
  return size + 2 * qsize;
}

Encoder::Encoder(int width, int height, int fps,
                 std::function<void(std::unique_ptr<Buffer> buf, int b_key, int dts, int pts)> enc_callback)
  :state(State::READY), error("")
{
  callback = enc_callback;
  enc_param.width = width;
  enc_param.height = height;

  enc_param.i_frame = 0;
  /* Get default params for preset/tuning */
  if(x264_param_default_preset(&enc_param.param, "medium", NULL) < 0) {
    state = State::ERROR;
    error = "cannot open codec";
    return;
  }

  /* Configure non-default params */
  enc_param.param.i_csp = X264_CSP_I420;
  enc_param.param.i_width = width;
  enc_param.param.i_height = height;
  enc_param.param.b_vfr_input = 0;
  enc_param.param.b_repeat_headers = 1;
  enc_param.param.b_annexb = 1;
  enc_param.param.i_fps_num = fps;
  enc_param.param.i_bframe         = 5;
  enc_param.param.i_keyint_max     = 5;
  enc_param.param.b_intra_refresh  = 0;

  /* Apply profile restrictions. */
  if(x264_param_apply_profile(&enc_param.param, "high") < 0) {
    state = State::ERROR;
    error = "cannot open apply codec param";
    return;
  }

  if(x264_picture_alloc(&enc_param.pic, enc_param.param.i_csp, enc_param.param.i_width, enc_param.param.i_height) < 0) {
    error = "cannot alloc picture";
    state = State::ERROR;
    return;
  }

  enc_param.h = x264_encoder_open(&enc_param.param);
  if(!enc_param.h) {
    error = "cannot open encoder";
    x264_picture_clean(&enc_param.pic);
    state = State::ERROR;
    return;
  }

  enc_param.luma_size = width * height;
  enc_param.chroma_size = enc_param.luma_size / 4;
  enc_param.fin = true;

  enc_param.buf_i420 = new uint8_t[get_i420_framesize(width, height)];
}

Encoder::~Encoder()
{
  x264_encoder_close(enc_param.h);
  x264_picture_clean(&enc_param.pic);
  if(state != State::ERROR)
    delete[] enc_param.buf_i420;
}

bool Encoder::add_buf(std::unique_ptr<Buffer> buf)
{
  if(state == State::STOPPED)
    return false;
  if(!buf) {
    enc_param.fin = false;
    state = State::STOPPED;
    return false;
  }
  rgb_to_i420(enc_param.width, enc_param.height, reinterpret_cast<uint8_t*>(buf->buffer()), enc_param.buf_i420);

  memcpy(enc_param.pic.img.plane[0], enc_param.buf_i420, enc_param.luma_size  );
  memcpy(enc_param.pic.img.plane[1], enc_param.buf_i420+enc_param.luma_size, enc_param.chroma_size);
  memcpy(enc_param.pic.img.plane[2], enc_param.buf_i420+enc_param.luma_size+enc_param.chroma_size, enc_param.chroma_size);

  enc_param.pic.i_pts = enc_param.i_frame;
  ++enc_param.i_frame;
  enc_param.i_frame_size = x264_encoder_encode(enc_param.h, &enc_param.nal, &enc_param.i_nal, &enc_param.pic, &enc_param.pic_out);
  if (enc_param.i_frame_size < 0) {
    enc_param.fin = false;
    state = State::STOPPED;
    return false;
  } else if (enc_param.i_frame_size) {
    if(callback) {
      Buffer *buf = new Buffer(enc_param.i_frame_size);
      memcpy(buf->buffer(), enc_param.nal->p_payload, enc_param.i_frame_size);
      callback(
        std::unique_ptr<Buffer>(buf),
        enc_param.pic_out.b_keyframe,
        enc_param.pic_out.i_dts,
        enc_param.pic_out.i_pts
      );
    }
  }
  return true;
}

void Encoder::join()
{
  if(enc_param.fin) {
    while (x264_encoder_delayed_frames(enc_param.h))
    {
      enc_param.i_frame_size = x264_encoder_encode(enc_param.h, &enc_param.nal, &enc_param.i_nal, NULL, &enc_param.pic_out);
      if (enc_param.i_frame_size < 0)
        break;
      else if (enc_param.i_frame_size)
      {
        if(callback) {
          Buffer *buf = new Buffer(enc_param.i_frame_size);
          memcpy(buf->buffer(), enc_param.nal->p_payload, enc_param.i_frame_size);
          callback(
            std::unique_ptr<Buffer>(buf),
            enc_param.pic_out.b_keyframe,
            enc_param.pic_out.i_dts,
            enc_param.pic_out.i_pts
          );
        }
      }
    }
  }
}
