#ifndef ENCODER_H_DEF
#define ENCODER_H_DEF
#include <stdint.h>
#include <x264.h>
#include <functional>
#include <memory>
#include "buffer.h"

class Encoder {
  enum class State {
    ERROR, READY, STARTED, STOPPED
  };
  struct EncoderParameters {
    x264_param_t param;
    x264_picture_t pic;
    x264_picture_t pic_out;
    x264_t *h;
    int i_frame;
    int i_frame_size;
    x264_nal_t *nal;
    int i_nal;
    int luma_size;
    int chroma_size;
    bool fin;
    uint8_t *buf_i420;
    int width;
    int height;
  };
  static int rgb_to_i420(int width, int height, uint8_t *src, uint8_t *dst);
  static size_t get_i420_framesize(int width, int height);
  std::function<void(std::unique_ptr<Buffer>, int, int, int)> callback;
  State state;
  std::string error;
  EncoderParameters enc_param;
public:
  Encoder(int width, int height, int fps,
          std::function<void(std::unique_ptr<Buffer> buf, int b_key, int dts, int pts)> enc_callback);
  ~Encoder();
  bool add_buf(std::unique_ptr<Buffer> buf);
  void join();
};

#endif //ENCODER_H_DEF
