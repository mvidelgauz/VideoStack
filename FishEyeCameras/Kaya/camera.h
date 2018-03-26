#ifndef CAMERA_H_DEF
#define CAMERA_H_DEF
#include "buffer.h"

#define UPDATE_TIME_OUT 1000
#define GET_DATA_TIME_OUT 500

#include <memory>
#include <functional>
#define MAXBOARDS 4
#define STREAMBUFFERS 8
#include "KYFGLib.h"

class Camera {
  std::string last_error;
  int64_t width, height;
  bool debug;
  bool run;
  std::function<void(std::unique_ptr<Buffer> buf, Camera *camera)> rgb_callback;
  std::function<void(const std::string &err, Camera *camera)> err_callback;
  FGHANDLE handle[MAXBOARDS];
  CAMHANDLE camHandleArray[MAXBOARDS][4];						// there are maximum 4 cameras
  STREAM_HANDLE cameraStreamHandle;
  size_t frameDataSize, frameDataAligment;
  STREAM_BUFFER_HANDLE streamBufferHandle[STREAMBUFFERS] = {0};
  int iFrame;
  unsigned int currentGrabberIndex;
  static void Stream_callback_func(STREAM_BUFFER_HANDLE streamBufferHandle, void* userContext);
  int connectToGrabber(unsigned int grabberIndex);
  FGSTATUS startCamera(unsigned int grabberIndex, unsigned int cameraIndex);
  bool checkError(const FGSTATUS &s, const std::string &name);

public:
  Camera(bool debug):
    last_error(""), width(-1), height(-1), debug(debug),
    run(false) {}
  std::pair<int, int> get_image_size() {return {width, height};}
  void start_camera(std::function<void(std::unique_ptr<Buffer> buf, Camera *camera)> rgb_callback,
                    std::function<void(const std::string &err, Camera *camera)> err_callback, int fps);
  void stop() {run = false;}
};

#endif //CAMERA_H_DEF
