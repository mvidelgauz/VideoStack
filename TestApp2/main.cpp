//./mc_player_nnv -cnt gt.cntr gt2.ts   - save to the container
//./mc_player_nnv -pcnt gt.cntr         - play from the container
//set args -snd 127.0.0.1:1090 gt2.ts   - play gt2.ts and send to the server
//set args -rcv 127.0.0.1:1091          - play from the server


////set args -snd 139.162.33.21:1090 gt2.ts 
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <unistd.h>



#include <stdio.h>
#include <stdlib.h>
#include "vSystem/VS_Reader.h"
#include <GL/glx.h>
#include <json/json.h>
#include "../X264/vx264.h"

#ifdef _WIN	    
CRITICAL_SECTION    MainCriticalSection;
#else
extern char         *SelemDevice;
extern char         *SelemName;
pthread_mutex_t     MainCriticalSection;
#endif 
int                 GlobWidth  = 640.0;
int                 GlobHeight = 360;
Display             *display   = NULL;
int                 isUserWantsWindowToClose = 0 ;
char                exec_path[256];
Window              win;

pthread_t           hFileThread = NULL;
char                **Files = NULL;
int                 iNum = 0;
int                 iCurFile = 0;
int                 bEnableCuvid = true;
int                 bNoAudio     = false;
int                 iCurrent = 0;
int                 bStarted = false;
char                *sDevice;
int                 bDevice = 0;
int                 bUseFloat = 0;
int                 iFrames = 1024;
int                 iScheme = 1;
int                 iAvailMin = -1;
int                 bBigBuf = 1;
int                 TimeToSkip = 5;
int                 VolIrv = 100;
int                 bMapped = 0;
int                 bInitedTextures = false;
XVisualInfo         visualinfo;
const char          *xserver = getenv( "DISPLAY" ) ;
extern VS_Sound     *Sound;

class VR_Reader : public VS_Reader{
public:
        VR_Reader();        
virtual void        FrameUpdated(VS_Cache *Cache);
virtual void        OnPlayStart();
virtual int         IsCuvidEnabled(VS_Cache *Cache){return bEnableCuvid;};
};

VR_Reader::VR_Reader(){

}
X264           *RX264         = NULL;    //for saving the file to a container
X264           *SX264         = NULL;    //to stream the file to a server
VR_Reader      *Reader        = NULL;
VS_Cache       *Video         = NULL;
VS_Controller  *Controller    = NULL;
int            bFirstFrame    = 1;
int            bOnPlay        = 1;
int            bFps           = 0;
int            iNoDisplay     = 0;
int            bNoVideo       = false;
char           *SX264_Adr     = NULL;
char           *RX264FileName = NULL;
char           *PktFileName   = NULL;
char           *CntrFileName  = NULL;
char           *RcvAdr        = NULL;
void DisplayFrame(int bNewFrame);
void VR_Reader::OnPlayStart(){
       if(bOnPlay == 0)return;
       // create OpenGL context
       GLXContext glcontext = glXCreateContext( display, &visualinfo, 0, True ) ;
       if (!glcontext)
       {
         printf("X11 server '%s' does not support OpenGL\n", xserver ) ;
         exit(1) ;
       }
       glXMakeCurrent( display, win, glcontext ) ;

       GLenum error = glewInit();
       if(error != GLEW_OK){
          printf("error with glew init()\n");
       } 
       if(bNoVideo == false){
          XMapWindow( display, win) ;
          bMapped = 1;

       
          #ifdef _NO_NV
          Video->DisplayType  = 1;
          #endif
          if(Controller->VideoCache != NULL){
             if(bEnableCuvid == true){
                Video->InitCuvid(); 
                Video->DisplayType  = 2;              
             }else Video->DisplayType  = 1;
             Video->ProcessTextures();
             bInitedTextures = 2;
           }
           printf("Mapped\n");

        }
        bOnPlay = 0;
}
void VR_Reader::FrameUpdated(VS_Cache *Cache){
        DWORD   *Buf;
        if(isUserWantsWindowToClose == 1 || bMapped == 0)return;
        if(RX264 != NULL){
           Buf = Cache->GetCurFrameBuf();
           if(Buf != NULL){
              RX264->SaveFrame((int32_t*)Buf);
           }
        }
        if(SX264 != NULL){
           Buf = Cache->GetCurFrameBuf();
           if(Buf != NULL){
              SX264->SaveFrame((int32_t*)Buf);
           }
        }
        DisplayFrame(1);  
}




//parsing a json file with scnene/parameters description
Json::Reader        reader;
Json::Value         root;
/*
player list:
{
"files" : { "gt2.ts", "1.mp4"}
}
config file:
{
"device" : "device name"
}




*/
char *ReadFile(char *Path, unsigned int *SizeW) {
	FILE *f = fopen(Path, "rb");
	if (f == NULL) {
		printf("file reading error\n");
		return 0;
	}
	fseek(f, 0, SEEK_END);
	unsigned int Size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *pBuf = (char *)malloc(Size + 1);
	fread(pBuf, Size, 1, f);
	fclose(f);
	pBuf[Size] = 0;
	if (SizeW != NULL)
		SizeW[0] = Size;
	return pBuf;
}
int ParseJson(char *Path, int bPlayerList){
	Json::Value              Value;
	unsigned int             Size;
	int                      rt;
	const char               *sVal;
	Json::Value              s_file;
	Json::Value              s_item;

	char *pBuf = ReadFile(Path, &Size);
	if (pBuf == NULL) {
		printf("json parsing error");
		return 0;
	}
	if (reader.parse(pBuf, pBuf + Size, root) == false) {		
		printf("json parsing error");
		free(pBuf);
		return 0;
	}
    if(bPlayerList == 1){
      if(root.isMember("files") == true) {
		  Json::Value s_file = root.get("files", "");
          for(Json::ValueConstIterator it = s_file.begin(); it != s_file.end(); ++it) {
  	 	      iNum++;
  		  }
          Files = (char**)malloc(iNum);
          int i = 0;
   	      for(Json::ValueConstIterator it = s_file.begin(); it != s_file.end(); ++it) {
  	 	      const Json::Value &cur_val = *it;
              sVal = (char *)cur_val.asCString();
              rt = strlen(sVal);
              Files[i] = (char*)malloc(rt+1);
              strcpy(Files[i], sVal);
              i++;
  		  }
	  } 
    }else{
   	   if(root.isMember("device") == true) {
		  Value = root.get("device", "");
		  sVal = (char *)Value.asCString();
          rt = strlen(sVal);
          sDevice = (char*)malloc(rt+1);
          strcpy(sDevice, sVal);
          bDevice = 1;
	  }
      if(root.isMember("mix_dev") == true) {
		  Value = root.get("mix_dev", "");
		  sVal = (char *)Value.asCString();
          rt = strlen(sVal);
          SelemDevice = (char*)malloc(rt+1);
          strcpy(SelemDevice, sVal);
	  }
      if(root.isMember("mix_ch") == true) {
		  Value = root.get("mix_ch", "");
		  sVal = (char *)Value.asCString();
          rt = strlen(sVal);
          SelemName = (char*)malloc(rt+1);
          strcpy(SelemName, sVal);
	  }
      if(root.isMember("float") == true) {
         Value = root.get("float", "");
         bUseFloat = Value.asInt();
      }
      if(root.isMember("frames") == true) {
         Value = root.get("frames", "");
         iFrames = Value.asInt();
      }
      if(root.isMember("trsh_scheme") == true) {
         Value = root.get("trsh_scheme", "");
         iScheme = Value.asInt();
      }
      if(root.isMember("avail_min") == true) {
         Value = root.get("avail_min", "");
         iAvailMin = Value.asInt();
      }
      if(root.isMember("big_buf") == true) {
         Value = root.get("big_buf", "");
         bBigBuf = Value.asInt();
      }
	}

	
	
	return 1;
}
void MainLock(){
        #ifdef _WIN
	    EnterCriticalSection(&MainCriticalSection);
        #else
        pthread_mutex_lock(&MainCriticalSection);
        #endif
}
void MainUnlock(){
        #ifdef _WIN
	    LeaveCriticalSection(&MainCriticalSection);
        #else
        pthread_mutex_unlock(&MainCriticalSection);
        #endif
}
int *df2 = NULL;
void DisplayFrame(int bNewFrame){
        XWindowAttributes       gwa;
        float                   ratio;       
        int                     Err;
        
        MainLock(); 
        if(iNoDisplay == 1){
           MainUnlock();
           return;
        }
//printf("Display pts: %d\n", (int)Reader->disp_pts);
        glEnable(GL_TEXTURE_2D);
Err = glGetError();
        XGetWindowAttributes(display, win, &gwa);
Err = glGetError();
        glViewport(0, 0, gwa.width, gwa.height);
Err = glGetError();      
        ratio = (float)gwa.width / (float) gwa.height;

      
        Err = glGetError();
        glClear(GL_COLOR_BUFFER_BIT);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnable( GL_BLEND );
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();


        if(Video != NULL && bInitedTextures == 2){
            /*if(bNewFrame == false || Video->Reader->bPause == 1)Video->DisplayCurrent();
            else{
               Video->ProcessFirstPacket();
               Video->FinishFirstPacket(); 
               if(Video->CurFrameNum > 0)bFirstFrame = 0;
            }*/
            Video->DisplayCurFrame();
        }// if(Video != NULL)
        if(bNoVideo == false){
            //glColor3f(1.0f, 0.0, 0.0);
            // glRotatef((float) glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glBegin(GL_QUADS);
            if(Video == NULL){
               glTexCoord2f( 0.0, 1.0 );glVertex3f(-1.0f, -1.0f, 0.f);
               glTexCoord2f( 0.0, 0.0 );glVertex3f(-1.0f, 1.0f, 0.f);
               glTexCoord2f( 1.0, 0.0 );glVertex3f(1.0f, 1.0f, 0.f);
               glTexCoord2f( 1.0, 1.0 );glVertex3f(1.0f, -1.0f, 0.f);
            }else{
               /*vertical strips for 0.25
               float w1 = 0.0;
               float w2 = 1.0;
               float h1 = -0.5;
               float h2 = 1.5;*/
               /*horizontal strips for 0.25
               float w1 = -0.5;
               float w2 = 1.5;
               float h1 = 0.0;
               float h2 = 1.0;*/
               float w1 = 0.0;    //if the window ratio is the same to the movie ratio
               float w2 = 1.0;
               float h1 = 0.0;
               float h2 = 1.0;
               float rat, rat2;
               rat = (float)Video->Stream->c_context->width / (float)gwa.width;
               float NewHeight = (float)Video->Stream->c_context->height / rat;
               rat2 = (float)gwa.height / NewHeight;
               if(rat2 >= 1.0){
                  h1 = -(rat2 - 1.0) / 2.0;
                  h2 = 1.0 - h1;
               }else{
                  rat = (float)Video->Stream->c_context->height / (float)gwa.height;
                  float NewWidth = (float)Video->Stream->c_context->width / rat;
                  rat2 = (float)gwa.width / NewWidth;
                  w1 = -(rat2 - 1.0) / 2.0;
                  w2 = 1.0 - w1;
               }
               glTexCoord2f( w1, h2 );glVertex3fv(Video->Corners[0]);
               glTexCoord2f( w1, h1 );glVertex3fv(Video->Corners[1]);
               glTexCoord2f( w2, h1);glVertex3fv(Video->Corners[2]);
               glTexCoord2f( w2, h2 );glVertex3fv(Video->Corners[3]);
            }
            glEnd();
            #ifndef _NO_NV
            glBindTexture(GL_TEXTURE_TYPE, 0);
            #endif
            glDisable(GL_FRAGMENT_PROGRAM_ARB);
        }// if(Video != NULL)
       glXSwapBuffers(display, win);
       //if(bFirstFrame == 1){
       //   Reader->FrameUpdated(Video);
       //}
       MainUnlock();
}
double NsDiff(struct timespec* time_stop, struct timespec* time_start);
void NextFile();

void *playing_files(void * Arg){
    for(;;){
        if(Reader->bEnd == 1){
     printf("found bEnd == true\n");
           VS_Cache* MainCache = Reader->FindCache(true, VS_ANY);
           int Diff;
           for(;;){               
               if(MainCache->cLast < MainCache->cFirst) {
                  Diff = MainCache->cLen - MainCache->cLast + MainCache->cFirst;
               }else{
                  Diff = MainCache->cLast - MainCache->cFirst;
              }
              if(Diff > 0){
                 usleep(50);
                 continue;
              }       
              break;       
           }
printf("cache is played completely\n");
           Controller->IsAudioComplete();          
     printf("audio buffer is empty\n");
           iNoDisplay = 1;           	
           iCurFile++;
           if(RX264 != NULL)RX264->Finish();
           if(iCurFile >= iNum){
     printf("iCurFile >= iNum\n");
               exit(0);
           }
     printf("Reader->Free\n");
           Reader->Free();
     printf("Controller->Reset\n");
           Controller->Reset();
    Reader->bEnd = 0;
    Reader->audio_secs = 0;
           if(bNoVideo == true){
             NextFile();
          }else{
             printf("Sending MC_2\n");         
             XClientMessageEvent ev = {NULL};
             ev.type = ClientMessage;
             ev.window = win;
             ev.message_type = XInternAtom(display, "MC_2", False);
             ev.format = 32;
             ev.data.l[0] = 1;
             ev.data.l[1] = 14;
             //ev.data.l[1] = CurrentTime;
             ev.data.l[2] = ev.data.l[3] = ev.data.l[4] = 0;
             XSendEvent (display, win, False,
                NULL, (XEvent*)&ev);
             XFlush(display);  
          }
        }
        usleep(200);
    }
	return NULL;
}
void OnRelaseThreads(){
     MainLock(); 
     iNoDisplay = 1;
     MainUnlock();     
}
int OnEndOfFile(){
    return 0;
}
void OnPrepared(VS_Reader *This){
    //iNoDisplay = 0;
}
void NextFile(){

    printf("received MC_2\n");
    Reader->Source     = Files[iCurFile];
    Reader->Controller = Controller;
    printf("preparing new file: %s\n", Reader->Source);
    if(Reader->Prepare() != EXIT_SUCCESS){
       printf("Couldn't preare the stream '%s'\n", Files[iCurFile]) ;
       exit(1);
    }            
    Video = Reader->FindCache(-1, VS_VIDEO);                    
    if(Video != NULL){
       //Video->bVFlip       = bVFlip;
       // Video->bHFlip       = bHFlip;
       Video->bSwapRGB     = false;
       #ifndef _NO_NV
       //Video->bEnableCuvid = bEnableCuvid;
       if(bEnableCuvid == true)
          Video->InitCuvid();
       #endif
       
       
       int StreamCnt       = 1;
       int Columns         = 1;
       int Rows            = 1;
       //bFirstFrame         = true;
       Video->CalcCorners(0, 0, 0, Rows, Columns);
    }else bFirstFrame = false;
    if(Controller->VideoCache != NULL)
       Controller->VideoCache->ProcessTextures();
    Reader->Start();
    bStarted = true;
    iNoDisplay = 0;
    iCurrent = 1;
}
void PrintHelp(){
    printf("Use: mc_player [-fh] [-fv] [-d] [-ns] video file(s)\n"
           "-c {(sound) configuration file}\n"
           "-l {play list}\n"
           "-fv flip vertical\n"
           "-fh flip horizontal\n"
           "-vl {how much to increase/decrease the volume level on Up/Down arrows}\n"
           "-tw {target_width} {target_height}\n" 
           "-tpw target width and height are the power of two from source sizes\n"
           #ifndef _NO_NV
           "-d disable trying of using nvdec, default it's enabled\n"
           #endif
           "-st {how much seconds to skip of left/right}\n"
           "-na don't play sound, play video only\n"
           "-nv don't play video, play audio only\n"
           "-cnt {file name} save the video to a container file\n"
           "-sv {file name} save the video to a h264 file\n"
           "-pkt {file_name} save h264 frame sizes to a file\n"
           "-pcnt play from the container\n"
           "-rcv {network adr} play the stream from the server\n"
           "-snd {network adr} stream the file to the server\n"
           "Keys:\n" 
           "Left/Right arrows - seek backward/forward\n"
           "Up/Down arrows - increase/decrease volume level\n"
           "Backspace - mute/unmute\n"
           "Enter - make a frameshot\n"
           "Pause key - pause/play video\n"
           "Space - show/hide fps\n"
           "Escape - exit\n"
     );

}
void McExpose(int *isRedraw){
    isRedraw[0] = 1 ;
    if(bStarted == false){       
       Reader->Start();
       if(bEnableCuvid == true){
          //while(bOnPlay == 1)usleep(50);
          //bInitedTextures = false;
          //Video->InitCuvid();
          //if(Video->bCuvidExists == true){
           //  Video->Helper->InitTexture();
          //   Video->DisplayType  = 2; 
          //}
          
       }
       bStarted = true;
       iNoDisplay = 0;
       iCurrent = 1;    
   }
}

int main(int argc, char* argv[])
{
   int          bVFlip       = 0;
   int          bHFlip       = 0;
   int          bPower       = 0;
   int          TargetWidth  = 0;
   int          TargetHeight = 0;
   int          iContainer   = 0;
   VS_Carrier   *Carrier     = NULL;
   
   Files = (char**)malloc(argc);

   if(argc < 2){
      PrintHelp();
      return 0;
   }
   
   if(argc > 2){
      for(int i=1; i<argc; i++){
          if(strcmp(argv[i], "-fv") == 0){
             bVFlip = 1;
          }if(strcmp(argv[i], "-fh") == 0){
             bHFlip = 1;
          }else if(strcmp(argv[i], "-d") == 0){
             bEnableCuvid = false;
          }else if(strcmp(argv[i], "-na") == 0){
             bNoAudio = true;
          }else if(strcmp(argv[i], "-nv") == 0){
             bNoVideo = true;
          }else if(strcmp(argv[i], "-tpw") == 0){
             bPower = 1;
          }else if(strcmp(argv[i], "-pcnt") == 0){
             iContainer = 1;
          }else if(strcmp(argv[i], "-tw") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             TargetWidth = strtol(argv[i+1], NULL, 10);
             i++;
          }else if(strcmp(argv[i], "-th") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             TargetHeight = strtol(argv[i+1], NULL, 10);
             i++;
          }else if(strcmp(argv[i], "-st") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             TimeToSkip = strtol(argv[i+1], NULL, 10);
             i++;
          }else if(strcmp(argv[i], "-vl") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             VolIrv = strtol(argv[i+1], NULL, 10);
             i++;
          }else if(strcmp(argv[i], "-c") == 0){
             //configuration
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             ParseJson(argv[i+1], 0);
             i++;
          }else if(strcmp(argv[i], "-l") == 0){
             //play list
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             ParseJson(argv[i+1], 1);
             i++;
          }else if(strcmp(argv[i], "-sv") == 0){
             //play list
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             RX264FileName = argv[i+1];
             i++;
          }else if(strcmp(argv[i], "-snd") == 0){
             //play list
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             SX264_Adr = argv[i+1];
             i++;
          }else if(strcmp(argv[i], "-cnt") == 0){
             //play list
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             CntrFileName = argv[i+1];
             i++;
          }else if(strcmp(argv[i], "-pkt") == 0){
             //play list
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             PktFileName = argv[i+1];
             i++;
          }else if(strcmp(argv[i], "-rcv") == 0){
             //play list
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             RcvAdr = argv[i+1];
             i++;
          }else{
             Files[iNum] = argv[i];
             iNum++;
          }
      }
   }else{
      Files[iNum] = argv[1];
      iNum++;
   }


   XInitThreads();
   display = XOpenDisplay( 0 ) ;
   

   if(display == 0){
      printf("Could not establish a connection to X-server '%s'\n", xserver ) ;
      exit(1) ;
   }
   #ifdef _WIN
   InitializeCriticalSection(&MainCriticalSection);
   #else    
   pthread_mutex_init(&MainCriticalSection, NULL);
   #endif
   Controller             = new VS_Controller;
   Reader                 = new VR_Reader;
   Reader->bCustomCuvidInit = true;
   Reader->Source         = Files[0];
   Reader->Controller     = Controller;   
   Reader->ExtOnPrepared  = OnPrepared;
   Reader->bIgnoreVideo   = bNoVideo;
   Reader->bIgnoreAudio   = bNoAudio;

   if(RcvAdr != NULL){
      Reader->iContainer  = 2;
      Carrier             = (VS_Carrier*)new VS_TCPCarrier; 
      Reader->Source      = RcvAdr;        
   }else{
      Reader->iContainer  = iContainer;
   }
   Controller->iSyncType  = 1;
   Controller->SoundParams.iUseFloat  = bUseFloat;
   Controller->SoundParams.iFrames    = iFrames;
   Controller->SoundParams.iThrScheme = iScheme;
   Controller->SoundParams.iAvailMin  = iAvailMin;
   Controller->SoundParams.bBigBuf    = bBigBuf;
   
   if(iNum > 1){
      Reader->OnEnd             = OnEndOfFile; 
      Reader->ReleaseExtThreads = OnRelaseThreads; 
      pthread_create(&hFileThread, 0, playing_files,0);
   }
    if(bDevice == 1){       
       Controller->SoundParams.sDevice = sDevice;
   }
   
   if(bNoVideo == false){
      // query Visual for "TrueColor" and 32 bits depth (RGBA)
      
      Status st = XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &visualinfo);
      if(st == 0)st = XMatchVisualInfo(display, DefaultScreen(display), 24, TrueColor, &visualinfo);
   
      // create window
   
      GC       gc ;
      XSetWindowAttributes attr ;
      attr.colormap   = XCreateColormap( display, DefaultRootWindow(display), visualinfo.visual, AllocNone) ;
      attr.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask ;
      attr.background_pixmap = None ;
      attr.border_pixel      = 0 ;
      win = XCreateWindow(    display, DefaultRootWindow(display),
                           50, 300, GlobWidth, GlobHeight, // x,y,width,height : are possibly opverwriteen by window manager
                           0,
                           visualinfo.depth,
                           InputOutput,
                           visualinfo.visual,
                           CWColormap|CWEventMask|CWBackPixmap|CWBorderPixel,
                           &attr
                           ) ;

      XSelectInput(display, win, 
        ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask |
        ButtonPressMask | ButtonReleaseMask  | StructureNotifyMask 
        );

      gc = XCreateGC( display, win, 0, 0) ;

      // set title bar name of window
      XStoreName( display, win, "mc_player" ) ;

      // say window manager which position we would prefer
      XSizeHints sizehints ;
      sizehints.flags = PPosition | PSize ;
      sizehints.x     = 50 ;  sizehints.y      = 300 ;
      sizehints.width = 400 ; sizehints.height = 100 ;
      XSetWMNormalHints( display, win, &sizehints ) ;
      // Switch On >> If user pressed close key let window manager only send notification >>
      Atom wm_delete_window = XInternAtom( display, "WM_DELETE_WINDOW", 0) ;
      XSetWMProtocols( display, win, &wm_delete_window, 1) ;
      { 
      // change foreground color to brown
         XColor    xcol ;
         xcol.red   = 153 * 256 ;   // X11 uses 16 bit colors !
         xcol.green = 116 * 256 ;
         xcol.blue  = 65  * 256 ;
        // XAllocColor( display, attr.colormap, &xcol) ;
        // XGCValues gcvalues ;
        // gcvalues.foreground = xcol.pixel ;
        //   XChangeGC( display, gc, GCForeground, &gcvalues) ;
       }

       
   }//if(bNoVideo == false)
   //we use ARB buffer, it should be done after glewInit
   if(Reader->Prepare(Carrier, 1) != 1){
      printf("Couldn't preare the stream '%s'\n", Reader->Source ) ;
      exit(1) ;
   }

   Controller->SetAsCurrentSound();

   double RColor = 0.0;
   Video = Reader->FindCache(-1, VS_VIDEO);
   strcpy(exec_path, argv[0]);
   if(Video != NULL){
      if(bPower == 1)Video->iTargetMode = 2;
      else if(TargetWidth != 0){
         if(TargetHeight != 0)
              Video->iTargetMode = 1;
         else Video->iTargetMode = 3;
         Video->iTargetW = TargetWidth;
         Video->iTargetH = TargetHeight;
      }
      Video->bVFlip       = bVFlip;
      Video->bHFlip       = bHFlip;
      Video->bSwapRGB     = false;      
      //Video->bEnableCuvid = bEnableCuvid;
      
      int StreamCnt       = 1;
      int Columns         = 1;
      int Rows            = 1;
    
      Video->CalcCorners(0, 0, 0, Rows, Columns);

      if(RX264FileName != NULL || CntrFileName != NULL){
         RX264              = new X264();
         if(CntrFileName != NULL){
            RX264->FileName = CntrFileName;
            RX264->bContainer = true;
         }else{
            RX264->FileName = RX264FileName;
         }
         RX264->PktFileName = PktFileName;
         RX264->Width       = Video->Stream->c_context->width;
         RX264->Height      = Video->Stream->c_context->height;
         RX264->Fps         = Reader->fps;
         RX264->Duration    = Reader->GetDuration();
         RX264->Init();
      }
      if(SX264_Adr != NULL){
         SX264              = new X264();
         SX264->bContainer  = true;
         SX264->SendTo      = SX264_Adr;
         SX264->Width       = Video->Stream->c_context->width;
         SX264->Height      = Video->Stream->c_context->height;
         SX264->Fps         = Reader->fps;
         SX264->Duration    = Reader->GetDuration();
         SX264->Init();
      }
   }else bFirstFrame = false;
   char name[256];
   sprintf(name, "mc_player - %s", argv[1]);
   if(bNoVideo == false)
      XStoreName( display, win, name ) ;

   //Reader->Start();
   
   timespec        time_prev;
   timespec        time_stop;
   clock_gettime(CLOCK_MONOTONIC, &time_prev);

    if(bNoVideo == true){
       Reader->Start();
       for(;;){
          sleep(	1);
       }
   }
   
   fd_set in_fds;
   struct timeval tv;
   int x11_fd = ConnectionNumber(display);
   //while( !isUserWantsWindowToClose )
   for(;;)
   {
      int  isRedraw = 0 ;
      FD_ZERO(&in_fds);
      FD_SET(x11_fd, &in_fds);

      // Set our timer.  One second sounds good.
      tv.tv_usec = 25;
      tv.tv_sec = 0;
      int num_ready_fds = select(x11_fd + 1, &in_fds, NULL, NULL, &tv);
      if(num_ready_fds == 0){
         McExpose(&isRedraw);
//DisplayFrame(iCurrent);
         continue;
      }else if(num_ready_fds < 0){
         printf("X11 selection error\n");
         continue;
      }
//printf("int num_fd: %d\n", num_ready_fds);

      XEvent   event;      
      double   diff, fr_per_sec;
      double   cur_time = 0; 
      int      CurLevel;
     
      //while( XEventsQueued(display, QueuedAlready) > 0)
      //while(XNextEvent( display, &event) > 0)
      //for(;;)
      int EvCount = XEventsQueued(display,QueuedAfterReading); 
//printf("EvCount A:  %d\n", EvCount);
      while (EvCount > 0) 
      {
         // process event
//printf("XNextEvent\n");        
         XNextEvent( display, &event) ;
         EvCount = XEventsQueued(display,QueuedAfterReading); 
//printf("EvCount B:  %d\n", EvCount);          

         switch(event.type)
         {  // see 'man XAnyEvent' for a list of available events
         case ClientMessage:
                  // check if the client message was send by window manager to indicate user wants to close the window
                  if(event.xclient.message_type  == XInternAtom( display, "WM_PROTOCOLS", 1)
                        && event.xclient.data.l[0]  == XInternAtom( display, "WM_DELETE_WINDOW", 1)){      
                     isUserWantsWindowToClose = 1;                 
                     Controller->ShutdownSound();                     
                     break;
                  }
                  if(event.xclient.message_type  == XInternAtom( display, "MC_2", 1)){
                     NextFile();
	
                     break;
                  }
                  if(event.xclient.message_type  == XInternAtom( display, "MC_1", 1)){
if(bFps == 1)
printf("MC_1\n");
                     isRedraw = 1 ;
                     iCurrent = 1;
                     if(bFps == 1){
                        clock_gettime(CLOCK_MONOTONIC, &time_stop);
                        double tm = NsDiff(&time_stop, &time_prev);
                        fr_per_sec = (double)Video->CurFrameNum / tm;
                        printf("fps: %f\n", fr_per_sec);
                     }
                  }
                  break;
         case KeyPress:
                  if (XLookupKeysym(&event.xkey, 0) == XK_Escape)
                  {
                     isUserWantsWindowToClose = 1 ;
                     break;
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_space)
                  {
                     bFps ^= 1;
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_Left)
                  {
                     Reader->SeekBackward(TimeToSkip);
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_Right)
                  {
                     Reader-> 	SeekForward(TimeToSkip);
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_Pause)
                  {
                     Reader->Pause(2);
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_Up)
                  {
                     CurLevel = Controller->GetVolume();
                     CurLevel += VolIrv;
                     Controller->SetVolume(CurLevel);
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_Down)
                  {
                     CurLevel = Controller->GetVolume();
                     CurLevel -= VolIrv;
                     Controller->SetVolume(CurLevel);
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_BackSpace)
                  {
                     Controller->Mute(2);
                  }
                  if (XLookupKeysym(&event.xkey, 0) == XK_Return)
                  {
                     //make a frameshot
                     Video->WriteCurFrameTo("frame.png");
                     //write_png_file("frame.png", Video->GetCurFrameBuf(), Video->Stream->c_context->width, Video->Stream->c_context->height);
                  }
                  break ;
         case ConfigureNotify:
                  //printf("configure event: %d %d\n", event.xconfigure.width, event.xconfigure.height);
                  GlobWidth = event.xconfigure.width; GlobHeight = event.xconfigure.height;
                  break;
         case Expose:
                  McExpose(&isRedraw);
                  break ; 	

         default:
               // do nothing
               break ;
         }

         if(isUserWantsWindowToClose == 1)break;
         if(isRedraw)
         {
//printf("Current: %d\n", iCurrent);  
            if(bFirstFrame == 1 || Reader->bPause == true)iCurrent = 1;
           // DisplayFrame(iCurrent); 
            iCurrent = 0;
         }
//usleep(40);
       }

       if(isUserWantsWindowToClose == 1)break;
   }

   XDestroyWindow( display, win ) ; 
   win = 0 ;
   XCloseDisplay( display ) ; 
   display = 0 ;

   return 0 ;
}
