#ifndef X264_H
#define X264_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "v_module.h"
#include <x264.h>

typedef void FrameUpdate(uint8_t *buf, int sizebuf, int width, int height);
void *update_thread(void *void_ptr);

class X264
{
    friend void *update_thread(void *void_ptr);
public:
    enum TYPE_OUT {
       BGR,
       ARGB,
       I420
     };
private:

     int             Mode;
     uint8_t         *argb;
     uint8_t         *i420;
     uint8_t         *YUV_Buf;
     int             errcode;
     TYPE_OUT        tout;
     FILE            *fl;
     FILE            *fl_pkt;
     int             luma_size;
     int             chroma_size;
     x264_param_t    param;
     x264_picture_t  pic;
     x264_picture_t  pic_out;
     x264_t          *h;
     int             i_frame;
     int             i_frame_size;
     x264_nal_t      *nal;
     int             i_nal;
     int             Off;
     int             hSock;
     int             SendBuf(uint8_t* pBuf, int Size);
public:
 
     X264();
     ~X264();
     int             Init();
     int             SaveFrame(int32_t *Buf);
     int             Finish();
     size_t          Get_i420_framesize(int width, int height);
     int             Bgr_to_i420(uint8_t *src, uint8_t *dst, int width, int height);
     int             Bgra_to_i420(uint8_t *src, uint8_t *dst, int width, int height);
     int             Bgr_to_argb(uint8_t *src, uint8_t *dst, int width, int height);
     int             GetWidth() {return Width;}
     int             GetHeight() {return Height;}
     void            GetMode(char *mode);
     char            *FileName;
     char            *PktFileName;
     char            *SendTo;      //the address where to send packets
     double          Fps;
     double          Duration;
     int             Width;
     int             Height;
     int             bRGB;
     int             bContainer;    //records to the own container
};

#endif // X264_H
