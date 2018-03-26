#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include "converter.h"
#include "camera.h"
#include "encoder.h"

#include <thread>
#include <queue>
#include <mutex>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "bayer.h"
#include <signal.h>

bool static signalstop = false;
void my_handler(int signum)
{
    if (signum == SIGUSR1)
      std::cout << "Received SIGUSR1!" << std::endl;
    else if (signum == SIGTERM)
      std::cout << "Received SIGTERM!" << std::endl;
    signalstop = (signum == SIGUSR1 || signum == SIGTERM);
}


FileParameters parse_ini(const std::string &fname) {
  FileParameters ret;
  std::ifstream ini(fname);
  if(ini.is_open()) {
    std::string line;
    size_t index_eq;
    while (std::getline(ini, line)) {
      index_eq = line.find('=');
      if(index_eq != std::string::npos) {
        std::string parameter = line.substr(0, index_eq);
        std::string value = line.substr(index_eq+1);
        std::istringstream valuestream(value);
        if(value.empty())
          continue;
        try {
          if(parameter == "OUTPUT_SIZE_HEIGHT") {
            valuestream >> ret.output_height;
            continue;
          }
          if(parameter == "ANTIALIASING_LEVEL") {
            valuestream >> ret.antialiasing;
            continue;
          }
          if(parameter == "FISHEYE_DEGREES") {
            valuestream >> ret.camera_fov;
            continue;
          }
          if(parameter == "OUTPUT_DEGREES") {
            valuestream >> ret.output_fov;
            continue;
          }
          if(parameter == "CORRECTION") {
            for(int i = 0; i < 4; ++i) {
              valuestream >> ret.correction[i];
            }
            continue;
          }
          if(parameter == "DEBUG") {
            int v;
            valuestream >> v;
            ret.debug = (v == 1) ? true : false;
            continue;
          }
          if(parameter == "FPS") {
            valuestream >> ret.fps;
            continue;
          }
          if(parameter == "SOCKET") {
            ret.socket = value;
            continue;
          }
          if(parameter == "FILE") {
            ret.file = value;
            continue;
          }
          if(parameter == "DEBAYER") {
            int v;
            valuestream >> v;
            ret.debayer = (v == 1) ? true : false;
            continue;
          }
          if(parameter == "CONVERTER") {
            int v;
            valuestream >> v;
            ret.type = (v == 1) ? FileParameters::TypeConverter::Spherical : FileParameters::TypeConverter::Perspective;
            continue;
          }
          if(parameter == "CAMRX") {
            valuestream >> ret.camera_radiusx;
            continue;
          }
          if(parameter == "CAMRY") {
            valuestream >> ret.camera_radiusy;
            continue;
          }
          if(parameter == "MAXQUEUE") {
            valuestream >> ret.maxqueue;
            continue;
          }
        } catch (...) { }
      }
    }
    ini.close();
  }
  return ret;
}

#define MAX_THREADS 4

int send_buf(int hSock, uint8_t* pBuf, int Size){
  int Len = Size;
  while(Len > 0){
    int bread = send(hSock, pBuf, Len,  MSG_NOSIGNAL);
    if(bread == -1){
     std::cerr << "send failed: " << std::strerror(errno) << std::endl;
     return -1;
    }
    pBuf = pBuf + bread;
    Len -= bread;
  }
  return Len;
}

int main(int argc, char **argv) {
  signal(SIGUSR1, my_handler);
  signal(SIGTERM, my_handler);
  std::chrono::steady_clock::time_point s, e, s2, e2;
  if(argc != 2) {
    std::cout << "Example:" << std::endl;
    std::cout << "   get_h264 parameters_file.ini" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "exit.." << std::endl;
    return 0;
  }

  FileParameters parameters = parse_ini(argv[1]);
  Encoder *enc = nullptr;
  s2 = s = std::chrono::steady_clock::now();


  /* -- Save to socket ------------------------- */
  bool save2socket = false;
  int hSock = 0;
  if(!parameters.socket.empty()){
    sockaddr_in adr;
    int port = 80;
    hSock = socket(AF_INET , SOCK_STREAM , 0);
    if(hSock == -1){
      std::cerr << "Could not create socket" << std::endl;
    }else{
      size_t colon = parameters.socket.find(':');
      if(colon != std::string::npos) {
        std::istringstream str(parameters.socket.substr(colon+1));
        str >> port;
      }
      adr.sin_addr.s_addr = inet_addr(parameters.socket.substr(0, colon).c_str());
      adr.sin_family = AF_INET;
      adr.sin_port = htons(port);
      if(connect(hSock , (struct sockaddr *)&adr , sizeof(adr)) < 0){
        std::cerr << "Socket connect failed: " << parameters.socket << std::endl;
        std::cerr << "Socket port: " << port << std::endl;
      }else{
        save2socket = true;
        std::cout << "Socket connected" << std::endl;
      }
    }
  }
  /* ------------------------------------------ */



  /* -- Save to file for test (TODO: delete) -- */
  bool save2file = false;
  std::ofstream h264file(parameters.file, std::ios::binary);
  if(!h264file.is_open())
    std::cerr << "Can't open file: " << parameters.file << std::endl;
  else
    save2file = true;
  /* ------------------------------------------ */

  Camera cam(parameters.debug);
  cam.start_camera(
  [&](std::unique_ptr<Buffer> buffer, Camera *camera)
  {
    if(signalstop) {
      camera->stop();
      enc->stop();
      return;
    }
    if(!enc) {
      std::pair<int,int> size = camera->get_image_size();
      if(size.first <= 0 || size.second <= 0) {
        std::cout << "no size, ignore buffer " << size.first << "x" << size.second << std::endl;
        return;
      }
      std::cout << "buffer size " << size.first << "x" << size.second << std::endl;
      parameters.set_image_size(size.first, size.second);

      /* --------- init socket ------ */
      if(save2socket) {
        int32_t Cnt[14];
        Cnt[0] = 12 * 4;
        Cnt[1] = 0xFFFFFFFF;
        Cnt[2] = 1;  //number of streams
        Cnt[3] = 1;  //packet type
        Cnt[4] = 1;  //stream id
        Cnt[5] = 1;  //codec id - h264
        Cnt[6] = parameters.output_width;
        Cnt[7] = parameters.output_height;
        ((double*)(Cnt+8))[0]  = parameters.fps;
        ((double*)(Cnt+10))[0] = -1;
        ((double*)(Cnt+12))[0] = 0.0;
        send_buf(hSock, (uint8_t*)Cnt, 14*4);
      }
      /* ---------------------------- */

      enc = new Encoder(parameters, 6, [&](std::unique_ptr<Buffer> buffer, int b_key, int dts, int pts){
          e = std::chrono::steady_clock::now();
          std::cout << "Get next image time (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << std::endl;

          /* -- Save to file for test (TODO: delete) -- */
          if(save2file)
            h264file.write(buffer->buffer(), buffer->size());
          /* ------------------------------------------ */

          /* ---SEND TO SERVER CODE HERE--- */
          if(save2socket) {
            int32_t Cnt[8];
            Cnt[0] = (8 - 2) * 4 + buffer->size();
            Cnt[1] = 1;  //stream id
            ((int64_t*)&Cnt[2])[0] = pts;
            ((int64_t*)&Cnt[4])[0] = dts;
            Cnt[6] = b_key;
            Cnt[7] = 0;
            uint8_t* out = new uint8_t[Cnt[0] + 8];
            memcpy(out, &Cnt[0], 8 * 4);
            memcpy(out + 8*4, buffer->buffer(), buffer->size());
            send_buf(hSock, out, Cnt[0] + 8);
            delete [] out;
          }
          /* ------------------------------ */

          s = std::chrono::steady_clock::now();
      });
      if(enc->get_state() == Encoder::State::ERROR) {
        std::cerr << "ERROR: " << enc->last_error() << std::endl;
        camera->stop();
        enc->stop();
      }
    }
    e2 = std::chrono::steady_clock::now();
    std::cout << "Get image from camera (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(e2 - s2).count() << std::endl;
    enc->add_buf(std::move(buffer));
    s2 = std::chrono::steady_clock::now();
  },
  [](const std::string &err, Camera *camera)
  {
    std::cerr << err << std::endl;
  }, parameters.fps);

  enc->join();

  std::cout << "Exiting ";

  for(int i = 0; i < 5; ++i) {
    std::cout << "..." << std::to_string(5-i);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  /* -- Save to file for test (TODO: delete) -- */
  if(save2file && h264file.is_open())
    h264file.close();
  /* --------------------------- */
  if(enc) delete enc;
  std::cout << std::endl << "Exit normal" << std::endl;

  return 0;
}
