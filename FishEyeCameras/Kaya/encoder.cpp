#include "encoder.h"
#include <string.h>
#include <iostream>

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

bool Encoder::add_to_encode(std::unique_ptr<Buffer> buf)
{
  if(state == State::STOPPED || !buf)
    return false;

  rgb_to_i420(parameters.output_width, parameters.output_height, reinterpret_cast<uint8_t*>(buf->buffer()), enc_param.buf_i420);

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

bool Encoder::syncfps()
{
  std::chrono::steady_clock::time_point now_time = std::chrono::steady_clock::now();
  int ms_delay = frame_time_min - std::chrono::duration_cast<std::chrono::milliseconds>(now_time - last_frame_time).count();
  if(ms_delay > 0) {
    if(parameters.debug)
      std::cout << "Frame dropped by sync func" << std::endl;
    return false;
  }
  last_frame_time = now_time;
  return true;
}

Encoder::Encoder(const FileParameters &parameters, int maxthreads,
                 std::function<void(std::unique_ptr<Buffer> buf, int b_key, int dts, int pts)> enc_callback)
  : fe(nullptr), converter(nullptr), parameters(parameters), maxthreads(maxthreads), frame_time_min(1000/parameters.fps),
    last_frame_time(std::chrono::steady_clock::now()), state(State::READY), error("")
{
  callback = enc_callback;

  enc_param.i_frame = 0;
  /* Get default params for preset/tuning */
  if(x264_param_default_preset(&enc_param.param, "medium", NULL) < 0) {
    state = State::ERROR;
    error = "cannot open codec";
    return;
  }

  /* Configure non-default params */
  enc_param.param.i_csp = X264_CSP_I420;
  enc_param.param.i_width = parameters.output_width;
  enc_param.param.i_height = parameters.output_height;
  enc_param.param.b_vfr_input = 0;
  enc_param.param.b_repeat_headers = 1;
  enc_param.param.b_annexb = 1;
  enc_param.param.i_fps_num = parameters.fps;
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

  enc_param.luma_size = parameters.output_width * parameters.output_height;
  enc_param.chroma_size = enc_param.luma_size / 4;
  enc_param.fin = true;

  enc_param.buf_i420 = new uint8_t[get_i420_framesize(parameters.output_width, parameters.output_height)];

  converter = new BayerToRgb(BayerToRgb::DC1394_8BIT, parameters.output_width, parameters.output_height, BayerToRgb::DC1394_COLOR_FILTER_RGGB, BayerToRgb::DC1394_BAYER_METHOD_NEAREST);

  fe = new FishEye(parameters);

  if(!fe->create_mesh()) {
    error = "create mesh error";
    x264_picture_clean(&enc_param.pic);
    state = State::ERROR;
    return;
  }
  state = State::STARTED;
  encode_thread = std::thread([this] {
    if(this->parameters.debug)
      std::cout << "Encode thread started" << std::endl;
    bool wait = false;
    int waittime = 1000/this->parameters.fps;
    while (state == State::STARTED) {
      if(wait)
        std::this_thread::sleep_for(std::chrono::milliseconds(waittime));
      std::lock_guard<std::mutex> lock(synccnvbuf);
      wait = convertedbuffers.empty();
      if(!wait) {
        this->add_to_encode(std::move(convertedbuffers.front()));
        convertedbuffers.pop();
        if(this->parameters.debug)
          std::cout << "wait encode buffers: " << std::to_string(convertedbuffers.size()) << std::endl;
      }
    }
    if(this->parameters.debug)
      std::cout << "Encode thread stopped" << std::endl;
  });

  convert_thread = std::thread([this] {
    if(this->parameters.debug)
      std::cout << "Convert thread started" << std::endl;
    bool wait = false;
    int waittime = 1000/this->parameters.fps;
    std::vector<std::thread> thread_pool;
    std::vector<std::unique_ptr<Buffer>> thread_result;
    int max_convert_threads = this->maxthreads - 2;
    for(int i = 0; i < max_convert_threads; ++i) {
      thread_pool.push_back(std::thread());
      thread_result.push_back(std::unique_ptr<Buffer>(nullptr));
    }
    int current_thread = 0;
    while (state == State::STARTED) {
      if(wait)
        std::this_thread::sleep_for(std::chrono::milliseconds(waittime));
      std::unique_ptr<Buffer> buf = nullptr;
      syncrawbuf.lock();
      wait = rawbuffers.empty();
      if(!wait) {
        buf = std::move(rawbuffers.front());
        rawbuffers.pop();
        if(this->parameters.debug)
          std::cout << "wait raw buffers: " << std::to_string(rawbuffers.size()) << std::endl;
      }
      syncrawbuf.unlock();
      if(buf) {
        /*uint8_t *rgbbuf = new uint8_t[this->parameters.camera_width*this->parameters.camera_height*3];
        bool cnv = converter->convert(reinterpret_cast<uint8_t*>(buf->buffer()), rgbbuf);
        if(cnv) {
          auto res = fe->convert(reinterpret_cast<char*>(rgbbuf));
          convertedbuffers.push(std::move(res));
        }
        delete [] rgbbuf;*/
        if(thread_pool[current_thread].joinable()) {
          thread_pool[current_thread].join();
          if(thread_result[current_thread]) {
            std::lock_guard<std::mutex> lock(synccnvbuf);
            convertedbuffers.push(std::move(thread_result[current_thread]));
            thread_result[current_thread] = nullptr;
          }
        }
        thread_pool[current_thread] = std::thread(
          [this, &thread_result](std::unique_ptr<Buffer> &&buf, int current_tread)->void{
               bool cnv = false;
               uint8_t *rgbbuf;
               if(this->parameters.debayer) {
                 rgbbuf = new uint8_t[this->parameters.camera_width*this->parameters.camera_height*3];
                 cnv = converter->convert(reinterpret_cast<uint8_t*>(buf->buffer()), rgbbuf);
               } else {
                 cnv = true;
                 rgbbuf = reinterpret_cast<uint8_t*>(buf->buffer());
               }
               if(cnv) {
                 auto res = fe->convert(reinterpret_cast<char*>(rgbbuf));
                 thread_result[current_tread] = std::move(res);
               } else {
                 if(this->parameters.debug)
                   std::cerr << "convert error" << std::endl;
                 thread_result[current_tread] = nullptr;
               }
               if(this->parameters.debayer)
                 delete [] rgbbuf;
             }
            , std::move(buf), current_thread
        );
        ++current_thread;
        if(current_thread >= max_convert_threads)
          current_thread = 0;
      }
    }

    for(int i = 0; i < max_convert_threads; ++i) {
      if(thread_pool[i].joinable()) {
        thread_pool[i].join();
        if(thread_result[i]) {
          std::lock_guard<std::mutex> lock(synccnvbuf);
          convertedbuffers.push(std::move(thread_result[i]));
          thread_result[i] = nullptr;
        }
      }
    }

    if(this->parameters.debug)
      std::cout << "Convert thread stopped" << std::endl;
  });
}

Encoder::~Encoder()
{
  //join();
  x264_encoder_close(enc_param.h);
  x264_picture_clean(&enc_param.pic);
  if(state != State::ERROR)
    delete[] enc_param.buf_i420;
  if(fe) delete fe;
  if(converter) delete converter;
}

void Encoder::add_buf(std::unique_ptr<Buffer> buf)
{
  if(state != State::STARTED) return;
  if(!syncfps()) {
    return;
  }
  std::lock_guard<std::mutex> lock(syncrawbuf);
  if(rawbuffers.size() > parameters.maxqueue) {
    if(parameters.debug)
      std::cout << "Frame dropped rawbuffer size=" << rawbuffers.size() << std::endl;
    return;
  }
  rawbuffers.push(std::move(buf));
}

void Encoder::join()
{
  state = State::STOPPED;
  /* join convert thread */
  if(convert_thread.joinable())
    convert_thread.join();
  /* join encode thread */
  if(encode_thread.joinable())
    encode_thread.join();
  /* flush encoder */
  if(enc_param.fin) {
    enc_param.fin = false;
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

void Encoder::stop()
{
  state = State::STOPPED;
}
