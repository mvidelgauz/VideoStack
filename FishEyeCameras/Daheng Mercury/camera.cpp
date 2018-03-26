#include "camera.h"
#include <iostream>

void Camera::show_error(GX_STATUS error_status)
{
  char* error_info = NULL;
  size_t size = 0;
  GX_STATUS status = GX_STATUS_ERROR;
  status = GXGetLastError(&error_status, NULL, &size);
  if(size < 1)
    return;
  error_info = new char[size];
  if(NULL == error_info) {
    last_error = "ShowErrorString memory error";
    if(debug)
      std::cerr << last_error << std::endl;
    return;
  }
  status = GXGetLastError(&error_status, error_info, &size);
  if (status != GX_STATUS_SUCCESS)
    last_error = "GXGetLastError error";
  else
    last_error = error_info;
  std::cerr << last_error << std::endl;
  delete[] error_info;
}

bool Camera::status_error(GX_STATUS error_status)
{
  if(error_status == GX_STATUS_SUCCESS)
    return false;
  show_error(error_status);
  return true;
}

GX_STATUS Camera::set_pixel_format_8bit(GX_DEV_HANDLE device_handle)
{
  GX_STATUS status = GX_STATUS_SUCCESS;
  int64_t pixel_size = 0;
  uint32_t enum_entry = 0;
  size_t buffer_size = 0;
  GX_ENUM_DESCRIPTION *enum_description = NULL;

  status = GXGetEnum(device_handle, GX_ENUM_PIXEL_SIZE, &pixel_size);
  if (status != GX_STATUS_SUCCESS) return status;
  if (pixel_size == GX_PIXEL_SIZE_BPP8) return GX_STATUS_SUCCESS;
  else {
    status = GXGetEnumEntryNums(device_handle, GX_ENUM_PIXEL_FORMAT, &enum_entry);
    if(status != GX_STATUS_SUCCESS) return status;

    buffer_size = enum_entry*sizeof(GX_ENUM_DESCRIPTION);
    enum_description = new GX_ENUM_DESCRIPTION[enum_entry];

    status = GXGetEnumDescription(device_handle, GX_ENUM_PIXEL_FORMAT, enum_description, &buffer_size);
    if(status != GX_STATUS_SUCCESS) {
      if(enum_description != NULL) {
        delete []enum_description;
        enum_description = NULL;
      }
      return status;
    }
    for(uint32_t i = 0; i < enum_entry; i++) {
      if((enum_description[i].nValue & GX_PIXEL_8BIT) == GX_PIXEL_8BIT) {
        status = GXSetEnum(device_handle, GX_ENUM_PIXEL_FORMAT, enum_description[i].nValue);
        break;
      }
    }
    if(enum_description != NULL) {
      delete[] enum_description;
      enum_description = NULL;
    }
  }
  return status;
}

void Camera::start_camera(std::function<void (std::unique_ptr<Buffer> buf, Camera *)> rgb_callback,
                          std::function<void (const std::string &, Camera *)> err_callback)
{
  GX_STATUS status = GX_STATUS_SUCCESS;
// init library
  if(status_error(GXInitLib())){
    err_callback(last_error, this);
    return;
  }
// open device
  unsigned device_number = 0;
  status = GXUpdateDeviceList(&device_number, UPDATE_TIME_OUT);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  if(device_number <= 0) {
    last_error = "Error: No devices";
    if(debug)
      std::cerr << last_error << std::endl;
    err_callback(last_error, this);
    return;
  }
  GX_OPEN_PARAM open_param;
  open_param.accessMode = GX_ACCESS_EXCLUSIVE;
  open_param.openMode = GX_OPEN_INDEX;
  open_param.pszContent = &pszContent_param;
  GX_DEV_HANDLE device_handle;
  status = GXOpenDevice(&open_param, &device_handle);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }

// init device
  status = GXSetEnum(device_handle,GX_ENUM_ACQUISITION_MODE, GX_ACQ_MODE_CONTINUOUS);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  // turn on
  status = GXSetEnum(device_handle,GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_ON);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  // set pixel format 8bit
  if(status_error(set_pixel_format_8bit(device_handle))){
    err_callback(last_error, this);
    return;
  }
  // source
  status = GXSetEnum(device_handle,GX_ENUM_TRIGGER_SOURCE, GX_TRIGGER_SOURCE_SOFTWARE);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }

// get device parameters
  bool is_implemented = false;
  int64_t pixel_color;
  status = GXIsImplemented(device_handle, GX_ENUM_PIXEL_COLOR_FILTER, &is_implemented);
  if(is_implemented) {
    status = GXGetEnum(device_handle, GX_ENUM_PIXEL_COLOR_FILTER, &pixel_color);
    if(status_error(status)){
      err_callback(last_error, this);
      return;
    }
  }
  int64_t payload_sz = 0;
  status = GXGetInt(device_handle, GX_INT_WIDTH, &width);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  status = GXGetInt(device_handle, GX_INT_HEIGHT, &height);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  status = GXGetInt(device_handle, GX_INT_PAYLOAD_SIZE, &payload_sz);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  if(debug) {
    std::cout << "camera width: " << width << std::endl;
    std::cout << "camera height: " << height << std::endl;
    std::cout << "camera payload size: " << payload_sz << std::endl;
  }

// start acquisition
  bool is_snap = false;
  status = GXSendCommand(device_handle,GX_COMMAND_ACQUISITION_START);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  is_snap = true;

// prepare buffer
  GX_FRAME_DATA frame;
  frame.pImgBuf = new char[payload_sz];
  frame.nHeight = height;
  frame.nWidth = width;
  frame.nPixelFormat = pixel_color;

// get data
  status = GXFlushQueue(device_handle);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  run = true;
  while (run) {
    status = GXSendCommand(device_handle,GX_COMMAND_TRIGGER_SOFTWARE);
    if(status_error(status)){
      err_callback(last_error, this);
      return;
    }
    status = GXGetImage(device_handle, &frame, GET_DATA_TIME_OUT);
    if(status_error(status)){
      err_callback(last_error, this);
      return;
    }
  // save data
    if(frame.nStatus != 0) {
      if(debug)
        std::cout << "Not complete image (GET_DATA_TIME_OUT too small)" << std::endl;
    } else {
      Buffer *buf = new Buffer(width*height*3);
      if(is_implemented)
        DxRaw8toRGB24(frame.pImgBuf, buf->buffer(), width, height, RAW2RGB_NEIGHBOUR, DX_PIXEL_COLOR_FILTER(pixel_color), false);
      else
        DxRaw8toRGB24(frame.pImgBuf, buf->buffer(), width, height, RAW2RGB_NEIGHBOUR, DX_PIXEL_COLOR_FILTER(NONE),false);

      rgb_callback(std::unique_ptr<Buffer>(buf), this);
    }
  }

//Close
  delete[] (char*)frame.pImgBuf;
  if(is_snap) { // stop acquisition
    status = GXSendCommand(device_handle, GX_COMMAND_ACQUISITION_STOP);
    if(status_error(status)){
      err_callback(last_error, this);
      return;
    }
  }
  status = GXCloseDevice(device_handle);
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
  status = GXCloseLib();
  if(status_error(status)){
    err_callback(last_error, this);
    return;
  }
}
