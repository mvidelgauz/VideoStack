#ifndef ENCODER_H_DEF
#define ENCODER_H_DEF
#include <stdint.h>
extern "C" {
#include <x264.h>
}
#include <functional>
#include <memory>
#include <queue>
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include "buffer.h"
#include "bayer.h"
#include "converter.h"

class Encoder {
public:
  enum class State {
    ERROR, READY, STARTED, STOPPED
  };
private:
  FishEye *fe;
  BayerToRgb *converter;
  const FileParameters parameters;
  const int maxthreads;
  int frame_time_min;
  std::chrono::steady_clock::time_point last_frame_time;

  std::mutex syncrawbuf, synccnvbuf;
  std::thread encode_thread, convert_thread;
  std::queue <std::unique_ptr<Buffer>> rawbuffers, convertedbuffers;

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
  };
  static int rgb_to_i420(int width, int height, uint8_t *src, uint8_t *dst);
  static size_t get_i420_framesize(int width, int height);
  std::function<void(std::unique_ptr<Buffer>, int, int, int)> callback;
  State state;
  std::string error;
  EncoderParameters enc_param;
  bool add_to_encode(std::unique_ptr<Buffer> buf);
  bool syncfps();
public:
  Encoder(const FileParameters &parameters, int maxthreads,
          std::function<void(std::unique_ptr<Buffer> buf, int b_key, int dts, int pts)> enc_callback);
  ~Encoder();
  State get_state() const {return state;}
  std::string last_error() const {return error;}
  void add_buf(std::unique_ptr<Buffer> buf);
  void join();
  void stop();
};

#endif //ENCODER_H_DEF
