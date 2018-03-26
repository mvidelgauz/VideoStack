#include "camera.h"
#include <string.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>

void* _aligned_malloc(size_t size, size_t alignment)
{
    size_t pageAlign = size % 4096;
    if(pageAlign)
    {
        size += 4096 - pageAlign;
    }

#if(GCC_VERSION <= 40407)
    void * memptr = 0;
    posix_memalign(&memptr, alignment, size);
    return memptr;
#else
    return aligned_alloc(alignment, size);
#endif
}

//#include <fstream>
void Camera::Stream_callback_func(STREAM_BUFFER_HANDLE streamBufferHandle, void *userContext)
{
  unsigned char* pFrameMemory = 0;
  Camera *cam = (Camera *) userContext;
  //int64_t totalFrames = -1;
  uint32_t frameId = 0;
  size_t sizeFr = 0;
  if (!streamBufferHandle) {
    // this callback indicates that acquisition has stopped
    return;
  }
  // process data associated with given stream buffer
  KYFG_BufferGetInfo(streamBufferHandle, KY_STREAM_BUFFER_INFO_BASE, &pFrameMemory, NULL, NULL);
  KYFG_BufferGetInfo(streamBufferHandle, KY_STREAM_BUFFER_INFO_ID, &frameId, NULL, NULL);
  KYFG_BufferGetInfo(streamBufferHandle, KY_STREAM_BUFFER_INFO_SIZE, &sizeFr, NULL, NULL);

  //totalFrames = KYFG_GetGrabberValueInt(cam->handle[cam->currentGrabberIndex], "RXFrameCounter");
  if(cam->debug)
    cam->err_callback("Good callback stream's buffer handle:" + std::to_string(streamBufferHandle)
               //+ ", ID:" + std::to_string(frameId) + ", total frames:" + std::to_string(totalFrames), cam);
               + ", ID:" + std::to_string(frameId) + ", size:" + std::to_string(sizeFr), cam);
  Buffer *buf = new Buffer(sizeFr);
  memcpy(buf->buffer(), pFrameMemory, sizeFr);

  /*--for debug--*/
  /*static int count = 0;
  if(count == 10) {
    count = -1;
    std::ofstream ff("tmp.raw", std::ios::binary);
    ff.write(buf->buffer(), sizeFr);
    ff.close();
  }
  if(count >=0 )
    ++count;*/
  /*--for debug--*/

  if(cam->run)
    cam->rgb_callback(std::unique_ptr<Buffer>(buf), cam);
  KYFG_BufferToQueue(streamBufferHandle, KY_ACQ_QUEUE_INPUT);
  // return stream buffer to input queue
}

int Camera::connectToGrabber(unsigned int grabberIndex)
{
  int64_t dmadQueuedBufferCapable;
  if ((handle[grabberIndex] = KYFG_Open(grabberIndex)) != -1) { // connect to selected device
    if(debug)
      err_callback("Good connection to grabber #" + std::to_string(grabberIndex) + " handle=" + std::to_string(handle[grabberIndex]), this);
  }
  else {
    last_error = "Could not connect to grabber #" + std::to_string(grabberIndex);
    err_callback(last_error, this);
    return -1;
  }
  dmadQueuedBufferCapable = KYFG_GetGrabberValueInt(handle[grabberIndex], DEVICE_QUEUED_BUFFERS_SUPPORTED);
  if (1 != dmadQueuedBufferCapable)
  {
    last_error = "grabber #" + std::to_string(grabberIndex) + " does not support queued buffers";
    err_callback(last_error, this);
    return -1;
  }
  currentGrabberIndex = grabberIndex;
  return 0;
}

FGSTATUS Camera::startCamera(unsigned int grabberIndex, unsigned int cameraIndex)
{
  FGSTATUS s;
  // put all buffers to input queue
  s = KYFG_BufferQueueAll(cameraStreamHandle, KY_ACQ_QUEUE_UNQUEUED, KY_ACQ_QUEUE_INPUT);
  if(s != FGSTATUS_OK) return s;
  // start acquisition
  s = KYFG_CameraStart(camHandleArray[grabberIndex][cameraIndex], cameraStreamHandle, 0);
  if(s != FGSTATUS_OK) return s;
  return FGSTATUS_OK;
}

bool Camera::checkError(const FGSTATUS &s, const std::string &name)
{
  if(FGSTATUS_OK != s) {
    err_callback("Error: " + name + " code=" + std::to_string(s), this);
    return false;
  }
  return true;
}

void Camera::start_camera(std::function<void (std::unique_ptr<Buffer> buf, Camera *)> rgb_callback,
                          std::function<void (const std::string &, Camera *)> err_callback, int fps)
{
  this->rgb_callback = rgb_callback;
  this->err_callback = err_callback;
  unsigned int* info = 0;
  unsigned int infosize = 0, grabberIndex = 0, cameraIndex = 0, i;
  int detectedCameras[MAXBOARDS];

  infosize = KYFG_Scan(0,0);						// First scan for device to retrieve the number of
                                        // virtual and hardware devices connected to PC
  if((info= (unsigned int*)malloc(sizeof(unsigned int)*infosize)) != 0) {
    infosize = KYFG_Scan(info,infosize);			// Scans for frame grabbers currently connected to PC. Returns array with each ones pid
  }
  else {
    err_callback("Info allocation failed", this);
  }

  if(debug) {
    err_callback("Number of scan results: " + std::to_string(infosize), this);
    for(i=0; i<infosize; i++)
    {
      err_callback("Device " + std::to_string(i) + ": " + KY_DeviceDisplayName(i), this);
    }
  }
  std::vector<void*> alignedbuffers;
  FGSTATUS s;
  if(connectToGrabber(grabberIndex) == 0) {
    // scan for connected cameras
    if(FGSTATUS_OK == KYFG_CameraScan(handle[grabberIndex], camHandleArray[grabberIndex], &detectedCameras[grabberIndex])) {
      if(debug)
        err_callback("Found " + std::to_string(detectedCameras[grabberIndex]) + " cameras", this);

      run = true;
      // open a connection to chosen camera
      if(FGSTATUS_OK == KYFG_CameraOpen2(camHandleArray[grabberIndex][0], 0)) {
        if(debug)
          err_callback("Camera 0 was connected successfully", this);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        // update camera/grabber buffer dimensions parameters before stream creation
        width = 5120;
        height = 5120;

        if(!checkError(KYFG_SetCameraValueInt(camHandleArray[grabberIndex][0], "Width", width), "Set camera Width")) {
          exit(1);
        }
        if(!checkError(KYFG_SetCameraValueInt(camHandleArray[grabberIndex][0], "Height", height), "Set camera Height")) {
          exit(1);
        }
        if(!checkError(KYFG_SetCameraValueEnum_ByValueName(camHandleArray[grabberIndex][0], "PixelFormat", "BayerRG8"), "Set camera Pixel format")) {
          exit(1);
        }

        if(!checkError(KYFG_SetCameraValueFloat(camHandleArray[grabberIndex][0], "AcquisitionFrameRate", fps), "Set camera frame rate")) {
          exit(1);
        }

        //added

        if(!checkError(KYFG_SetCameraValueEnum_ByValueName(camHandleArray[grabberIndex][0], "ExposureMode", "Timed"), "Set camera Exposure Mode")) {
          exit(1);
        }
        if(!checkError(KYFG_SetCameraValueFloat(camHandleArray[grabberIndex][0], "ExposureTime", 26265), "Set camera Exposure Time")) {
          exit(1);
        }
        if(!checkError(KYFG_SetCameraValueInt(camHandleArray[grabberIndex][0], "AnalogGainLevel", 0x10), "Set camera Analog Gain")) {
          exit(1);
        }
        if(!checkError(KYFG_SetCameraValueInt(camHandleArray[grabberIndex][0], "AnalogBlackLevel", 10), "Set camera Analog Black Level")) {
          exit(1);
        }
        if(!checkError(KYFG_SetGrabberValueEnum(handle[grabberIndex], "PixelFormat", 1025), "Set grabber Transformation Pixel Format")) { //RGB8
        //if(!checkError(KYFG_SetGrabberValueEnum(handle[grabberIndex], "PixelFormat", 0), "Set grabber Transformation Pixel Format")) { //BayerRG8
          exit(1);
        }
        //----

        // create stream and assign appropriate runtime acquisition callback function
        if(!checkError(KYFG_StreamCreate(camHandleArray[grabberIndex][cameraIndex], &cameraStreamHandle, 0), "Camera Stream create")) {
          exit(1);
        }
        if(!checkError(KYFG_StreamBufferCallbackRegister(cameraStreamHandle, Stream_callback_func, this), "Camera callback register")) {
          exit(1);
        }
        // Retrieve information about required frame buffer size and alignment
        if(!checkError(KYFG_StreamGetInfo(cameraStreamHandle, KY_STREAM_INFO_PAYLOAD_SIZE, &frameDataSize, NULL, NULL), "Camera Get info payload size")) {
          exit(1);
        }
        if(!checkError(KYFG_StreamGetInfo(cameraStreamHandle, KY_STREAM_INFO_BUF_ALIGNMENT, &frameDataAligment, NULL, NULL), "Camera Get info bufer alignment")) {
          exit(1);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if(debug)
          std::cout << "allocate memory for desired number of frame buffers" << std::endl;


        // allocate memory for desired number of frame buffers
        for (iFrame = 0; iFrame < STREAMBUFFERS; iFrame++)
        {
          if(debug)
            std::cout << "allocate memory for " << iFrame << " frame buffers size=" << frameDataSize << std::endl;
          void * pBuffer = _aligned_malloc(frameDataSize, frameDataAligment);
          alignedbuffers.push_back(pBuffer);
          if(!checkError(KYFG_BufferAnnounce(cameraStreamHandle, pBuffer, frameDataSize, NULL, &streamBufferHandle[iFrame]), "Camera buffer announce")) {
            exit(1);
          }
        }
        if(debug)
          std::cout << "camera start.." << std::endl;
        if(!checkError(startCamera(grabberIndex, 0), "Camera start")) {
          exit(1);
        }
        while (run) {
          /*std::string instr;
          if(std::cin >> instr) {
            if(!instr.empty() && instr == "exit") {
              break;
            }
            else {
              std::cout << "Type exit to quit!" << std::endl;
            }
          }*/
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
      else {
        err_callback("Camera isn't connected", this);
      }
    }
    else {
      err_callback("No cameras were detected", this);
    }
  }

  if(debug)
    std::cout << "Camera stopping.. " << std::endl;
  s = KYFG_CameraStop(camHandleArray[grabberIndex][0]);
  if(debug)
    std::cout << (s == FGSTATUS_OK ? "Camera 0 stopped": "Error while stopping camera 0: " + std::to_string(s)) << std::endl;

  if(debug)
    std::cout << "Camera callback unregister.. " << std::endl;
  s = KYFG_StreamBufferCallbackUnregister(cameraStreamHandle, Stream_callback_func);
  if(debug)
    std::cout << (s == FGSTATUS_OK ? "Camera 0 callback unregistered": "Error while callback unregistered camera 0: " + std::to_string(s)) << std::endl;

  if(debug)
    std::cout << "Camera stream deleting.. " << std::endl;
  s = KYFG_StreamDelete(cameraStreamHandle);
  if(debug)
    std::cout << (s == FGSTATUS_OK ? "Camera stream deleted": "Error while deleting camera stream: " + std::to_string(s)) << std::endl;

  if(debug)
    std::cout << "Camera closing.. " << std::endl;
  s = KYFG_CameraClose(camHandleArray[grabberIndex][0]);
  if(debug)
    std::cout << (s == FGSTATUS_OK ? "Camera 0 closed": "Error while closing camera 0: " + std::to_string(s)) << std::endl;

  if(debug)
    std::cout << "Grabber closing.. " << std::endl;
  s = KYFG_Close(handle[grabberIndex]);
  if(debug)
    std::cout << (s == FGSTATUS_OK ? "Grabber 0 closed": "Error while closing Grabber 0: " + std::to_string(s)) << std::endl;
  for(auto &v: alignedbuffers)
    free(v);
}
