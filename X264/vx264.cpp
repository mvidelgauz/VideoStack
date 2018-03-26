#include "vx264.h"
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

X264::X264()
{
  	Width    = 0;  
    Height   = 0;
    Mode     = 0;
    argb     = NULL;
    i420     = NULL;
    tout     = BGR;
    bRGB     = true;
    YUV_Buf  = NULL;
    Fps      = 30;
    FileName    = NULL;
    PktFileName = NULL;
    bContainer  = false;
    SendTo      = NULL;
    hSock       = -1;
    fl          = NULL;
}

X264::~X264()
{
}
int X264::SendBuf(uint8_t* pBuf, int Size){
    int Len = Size;

    while(Len > 0){             
          int bread = send(hSock, (char *) pBuf, Len,  MSG_NOSIGNAL);        
          if(bread == -1){
             return -1;
          }
          pBuf = (uint8_t*)((BYTE*)pBuf + bread);
          Len -= bread;
    }//while(len > 0) 
}
int X264::Init(){
    int Ret = 0;
    if(SendTo != NULL){
       sockaddr_in adr;
       int         port;
       hSock = socket(AF_INET , SOCK_STREAM , 0);
       if(hSock == -1){
          printf("Could not create socket");
       }else{
          char *pst2 = strchr(SendTo, ':');
          if(pst2 != NULL){
             pst2[0] = 0;
             pst2++;
             sscanf(pst2, "%d", &port);           
          }
          adr.sin_addr.s_addr = inet_addr(SendTo);
          adr.sin_family = AF_INET;
          adr.sin_port = htons(port);
          if(connect(hSock , (struct sockaddr *)&adr , sizeof(adr)) < 0){
             printf("connect failed. Error");       
          }else{
             Ret = 1;
          }
        }
    }
    if(FileName != NULL){
       fl = fopen(FileName, "w+");
       if(fl != NULL)Ret = 1;
    }
    if(PktFileName != NULL)
       fl_pkt = fopen(PktFileName, "w+");

    if(Ret == 0)return NULL;
    if(bContainer){
       DWORD Cnt[14];
       Cnt[0] = 12 * 4;
       Cnt[1] = 0xFFFFFFFF;
       Cnt[2] = 1;  //number of streams
       Cnt[3] = 1;  //packet type
       Cnt[4] = 1;  //stream id
       Cnt[5] = 1;  //codec id - h264
       Cnt[6] = Width;
       Cnt[7] = Height;
       ((double*)(Cnt+8))[0]  = Fps;
       ((double*)(Cnt+10))[0] = Duration;
       ((double*)(Cnt+12))[0] = 0.0;
       if(hSock != NULL)SendBuf((uint8_t*)Cnt, 14*4);
       if(fl != NULL)fwrite(Cnt, 1, 14*4, fl); 
    }


    i_frame = 0;



    /* Get default params for preset/tuning */
    if(x264_param_default_preset(&param, "medium", "zerolatency") < 0) {
       printf("cannot open codec\n");
       fclose(fl);
       return NULL;
    }

    /* Configure non-default params */
    param.i_csp            = X264_CSP_I420;
    param.i_width          = Width;
    param.i_height         = Height;
    param.b_vfr_input      = 0;
    param.b_repeat_headers = 1;
    param.b_annexb         = 1;
    param.i_fps_num        = Fps;
    param.i_bframe         = 5;
    param.i_keyint_max     = 5;
    param.b_intra_refresh  = 0;

    /* Apply profile restrictions. */
    if(x264_param_apply_profile(&param, "high") < 0) {
       printf("cannot open apply codec param\n");
       fclose(fl);
       if(PktFileName != NULL)fclose(fl_pkt);
       return NULL;
    }

    if(x264_picture_alloc(&pic, param.i_csp, param.i_width, param.i_height) < 0) {
       printf("cannot alloc picture\n");
       fclose(fl);
       if(PktFileName != NULL)fclose(fl_pkt);
       return NULL;
    }

    h = x264_encoder_open(&param);
    if(!h){
       printf("cannot open encoder\n");
       fclose(fl);
       if(PktFileName != NULL)fclose(fl_pkt);
       x264_picture_clean(&pic);
       return NULL;
    }

    luma_size = Width * Height;
    chroma_size = luma_size / 4;
    
    if(bRGB)
       YUV_Buf = (uint8_t*)malloc(luma_size + chroma_size * 2);
 
    Off = 0;

}
int X264::SaveFrame(int32_t *Buf){
    uint8_t *FrBuf;
    uint8_t *OutBuf;
    if(bRGB == false){
       FrBuf = (uint8_t*)Buf;
    }else{
       Bgra_to_i420((uint8_t*)Buf, YUV_Buf, Width, Height);
       FrBuf = YUV_Buf;
    }
    memcpy(pic.img.plane[0], FrBuf, luma_size);
    memcpy(pic.img.plane[1], FrBuf+luma_size, chroma_size);
    memcpy(pic.img.plane[2], FrBuf+luma_size+chroma_size, chroma_size);

    pic.i_pts = i_frame;
    ++i_frame;
    i_frame_size = x264_encoder_encode(h, &nal, &i_nal, &pic, &pic_out);
    if(i_frame_size < 0)
       return 0;
      
    if(i_frame_size){       
       if(bContainer){
          DWORD Cnt[8];
          printf("i_frame_size: %d, i_frame: %d, keyframe: %d\n", i_frame_size, i_frame, pic_out.b_keyframe);
          Cnt[0] = (8 - 2) * 4 + i_frame_size;
          Cnt[1] = 1;  //stream id
          ((int64_t*)&Cnt[2])[0] = pic.i_pts;
          //Cnt[2] = 0;  //pts
          //Cnt[3] = 0;
          //Cnt[4] = 0;  //dts
          //Cnt[5] = 0;
          ((int64_t*)&Cnt[4])[0] = pic.i_dts;
          if(pic_out.b_keyframe)Cnt[6] = 1;
          else Cnt[6] = 0;
          Cnt[7] = 0;
          if(fl != NULL)fwrite(Cnt, 1, 8*4, fl);
          if(hSock != NULL){
             OutBuf = (uint8_t*)malloc(Cnt[0] + 8);
             memcpy(OutBuf, &Cnt[0], 8 * 4);
             memcpy(OutBuf + 8*4, nal->p_payload,i_frame_size); 
             SendBuf(OutBuf, Cnt[0] + 8);
             free(OutBuf);
          }
       }
       if(fl != NULL){
          if(fwrite(nal->p_payload, 1, i_frame_size, fl) != i_frame_size){
             printf("error of frame writing\n");
             exit(0);
          } 
       }
       if(PktFileName != NULL){         
          Off += i_frame_size;
          fprintf(fl_pkt, "%d\n", i_frame_size);
          
       }
    }
    return 1;
}
int X264::Finish(){
    /* Flush delayed frames */
    while(x264_encoder_delayed_frames(h)){
          i_frame_size = x264_encoder_encode(h, &nal, &i_nal, NULL, &pic_out);
          if(i_frame_size < 0)
             break;
          else if(i_frame_size){
             if(fwrite(nal->p_payload, 1, i_frame_size, fl) != i_frame_size)
                break;
          }
    
    }
    x264_encoder_close(h);
    x264_picture_clean(&pic);
    fflush(fl);
    fclose(fl);
    if(PktFileName != NULL){
       fflush(fl_pkt);
       fclose(fl_pkt);
    }
    return NULL;
}



size_t X264::Get_i420_framesize(int width, int height)
{
    size_t size = (size_t)width * (size_t)height;
    size_t qsize = size / 4;
    return size + 2 * qsize;
}

int X264::Bgr_to_i420(uint8_t *src, uint8_t *dst, int width, int height)
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

    for(i = 0; i < height / 2; ++i){
        for(j = 0; j < width / 2; ++j){
            short r, g, b;
            b = *src_even++;
            g = *src_even++;
            r = *src_even++;

            *dst_y_even++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);
            *dst_u++ = (unsigned char)(((r * -38 - g * 74 + b * 112 + 128) >> 8) + 128);
            *dst_v++ = (unsigned char)(((r * 112 - g * 94 - b * 18 + 128) >> 8) + 128);
	
            r = *src_even++;
            g = *src_even++;
            b = *src_even++;

            *dst_y_even++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);

            r = *src_odd++;
            g = *src_odd++;
            b = *src_odd++;

            *dst_y_odd++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);

            r = *src_odd++;
            g = *src_odd++;
            b = *src_odd++;

            *dst_y_odd++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);
        }

        dst_y_odd += width;
        dst_y_even += width;
        src_odd += width * 3;
        src_even += width * 3;
    }

    return 0;
}
int X264::Bgra_to_i420(uint8_t *src, uint8_t *dst, int width, int height)
{
    int i, j;
    unsigned char *dst_y_even;
    unsigned char *dst_y_odd;
    unsigned char *dst_u;
    unsigned char *dst_v;
    const unsigned char *src_even;
    const unsigned char *src_odd;
    src_even = (const unsigned char *)src;
    src_odd = src_even + width * 4;

    dst_y_even = (unsigned char *)dst;
    dst_y_odd = dst_y_even + width;
    dst_u = dst_y_even + width * height;
    dst_v = dst_u + ((width * height) >> 2);

    for(i = 0; i < height / 2; ++i){
        for(j = 0; j < width / 2; ++j){
            short r, g, b, a;
            r = *src_even++;
            g = *src_even++;
            b = *src_even++;
            a = *src_even++;

            *dst_y_even++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);
            *dst_u++ = (unsigned char)(((r * -38 - g * 74 + b * 112 + 128) >> 8) + 128);
            *dst_v++ = (unsigned char)(((r * 112 - g * 94 - b * 18 + 128) >> 8) + 128);

            r = *src_even++;
            g = *src_even++;
            b = *src_even++;
            a = *src_even++;

            *dst_y_even++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);

            r = *src_odd++;
            g = *src_odd++;
            b = *src_odd++;
            a = *src_odd++;

            *dst_y_odd++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);

            r = *src_odd++;
            g = *src_odd++;
            b = *src_odd++;
            a = *src_odd++;

            *dst_y_odd++ = (unsigned char)(((r * 66 + g * 129 + b * 25 + 128) >> 8) + 16);
        }

        dst_y_odd += width;
        dst_y_even += width;
        src_odd += width * 4;
        src_even += width * 4;
    }

    return 0;
}

int X264::Bgr_to_argb(uint8_t *src, uint8_t *dst, int width, int height)
{
    for(int i = 0; i < width*height; ++i) {
        uint8_t &b  = *(src+i*3  );
        uint8_t &g  = *(src+i*3+1);
        uint8_t &r  = *(src+i*3+2);
        uint8_t &aD = *(dst+i*4  );
        uint8_t &rD = *(dst+i*4+1);
        uint8_t &gD = *(dst+i*4+2);
        uint8_t &bD = *(dst+i*4+3);
        aD = 0xff; rD = r; gD = g; bD = b;
    }
    return 0;
}


