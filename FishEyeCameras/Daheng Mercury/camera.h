#ifndef CAMERA_H_DEF
#define CAMERA_H_DEF
#include "GxIAPI.h"
#include "DxImageProc.h"
#include "buffer.h"

#define UPDATE_TIME_OUT 1000
#define GET_DATA_TIME_OUT 500

#include <memory>
#include <functional>

class Camera {
  char pszContent_param;
  std::string last_error;
  int64_t width, height;
  bool debug;
  bool run;

  void show_error(GX_STATUS error_status);
  bool status_error(GX_STATUS error_status);
  GX_STATUS set_pixel_format_8bit(GX_DEV_HANDLE device_handle);

public:
  Camera(bool debug): pszContent_param('1'), last_error(""), width(-1), height(-1), debug(debug), run(false) {}
  std::pair<int, int> get_image_size() {return {width, height};}
  void start_camera(std::function<void(std::unique_ptr<Buffer> buf, Camera *camera)> rgb_callback,
                    std::function<void(const std::string &err, Camera *camera)> err_callback);
  void stop() {run = false;}
};

#endif //CAMERA_H_DEF
