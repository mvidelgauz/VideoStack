//set args -sdur 1.0 -hlsv -ts 15 -tf 50 -i gt2.ts /var/www/html/videos/1
//set args -sdur 1.0 -hls -ts 15 -tf 50 -i gt2.ts /var/www/html/videos/1
//set args -btr 8000000 -sdur 1.0 -fps 30 -parts 5 -hls -is  /var/www/html/videos/1   -> streaming desktop to the nginx directory
//set args -hls -ts 15 -tf 50 -i gt2.ts
//set args -ts 15 -tf 50 -i gt2.ts gt4.ts
//set args -ts 15 -i gt2.ts gt3.ts
//set args -ts 15 -i gt2.ts -pic t5.png
#include "vSystem/VS_Writer.h"
#include <iostream>
#include <png.h>
#include <unistd.h>

void write_png_file(char* file_name, uint32_t *pBuf, int width, int height);
extern "C"{
void* (*fmp_malloc)(size_t size);
void* (*fmp_realloc)(void *pBuf, size_t size);
void  (*fmp_free)(void *ptr);
}

void PrintHelp(){
    printf("Use: mc_encoder -i video file(s) -o output video/picture\n"
           "-o for hls is output directory\n"
           "-i {input files}\n"
           "-is the input stream is the desktop $DISPLAY\n"
           "-l {play list}\n"
           "-ts {time in seconds or start time for crop}\n"
           "-tf {time in seconds where to finish}\n"
           "-ow {output width}\n"
           "-oh {output height}\n"
           "-btr {output bitrate}\n"
           "-pic - takes only one frame\n"
           "-hls - output to hls format\n"
           "-hlsv - output the live stream\n"
           "-fps - frame per second for output hls\n"
           "-parts {number of existing segments in m3u file}\n"
           "-sdur {sec.ms} - hls segment duration\n"          
     );
	
}
int        bPic = false;
char       *FileName = NULL;
VS_Writer  *Writer = NULL;
int        bReady = false;
int        bFirst = true;
void OnSeek(VS_Reader *Reader){
   bFirst = false;
   printf("OnSeek\n");
}
void OnFrame(VR_Reader *Reader){
  if(bPic == true && FileName != NULL){
     if(Writer->Reader->iWasSeek == 0 && bFirst == false && Writer->Reader->iSeekTs == -1){
        printf("writing to png\n");
        Writer->Reader->WritePngFile(FileName, Writer->FrameBuf, Writer->FrameWidth, Writer->FrameHeight);
        bReady = true;
     }
   }
}
int main(int argc, char **argv) {
  fmp_malloc = NULL;
  fmp_realloc = NULL;
  fmp_free = NULL;
  if(argc < 2){
      PrintHelp();
      return 0;
   }
   char     *st_input     = NULL;
   double   seg_duration  = 10.0;
   int      bVideo        = true;
   int      bHls          = false;
   int      bLive         = false;
   int      bDesktop      = false;
   int      bWasParts     = 0;
   int      HlsParts      = 0;
   int      OutputWidth   = 0, OutputHeight = 0;
   double   BeginTime     = 0;
   double   EndTime       = 0;
   int      EncFps        = 30;
   int      BitRate       = 40000;
   for(int i=1; i<argc; i++){
          if(strcmp(argv[i], "-i") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             st_input = argv[i+1];
             i++;
             continue;
          }else if(strcmp(argv[i], "-l") == 0){
            
          }else if(strcmp(argv[i], "-ts") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             BeginTime = atof(argv[i+1]);
             i++;
             continue;
          }else if(strcmp(argv[i], "-tf") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             EndTime = atof(argv[i+1]);
             i++;
             continue;
          }else if(strcmp(argv[i], "-fps") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             EncFps = strtol(argv[i+1], NULL, 10);
             i++;
             continue;
          }else if(strcmp(argv[i], "-btr") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             BitRate = strtol(argv[i+1], NULL, 10);
             i++;
             continue;
          }else if(strcmp(argv[i], "-ow") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             OutputWidth = strtol(argv[i+1], NULL, 10);
             i++;
             continue;
          }else if(strcmp(argv[i], "-oh") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
                OutputHeight = strtol(argv[i+1], NULL, 10);
             }
             i++;
             continue;
          }else if(strcmp(argv[i], "-parts") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);                
             }
             HlsParts = strtol(argv[i+1], NULL, 10);
             bWasParts = 1;
             i++;
             continue;
          }else if(strcmp(argv[i], "-pic") == 0){
             bPic = true;
          }else if(strcmp(argv[i], "-hls") == 0){
             bHls = true;
          }else if(strcmp(argv[i], "-is") == 0){
             bDesktop = true;
          }else if(strcmp(argv[i], "-hlsv") == 0){
             bHls = true;
             bLive = true;
             if(bWasParts == 0)HlsParts = 6;
          }else if(strcmp(argv[i], "-sdur") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             seg_duration = atof(argv[i+1]);
             i++;
             continue;          
          }else{
             FileName = argv[i];
          }
  }
  if((st_input == NULL && bDesktop == false) || (FileName == NULL && bHls == false)){
     PrintHelp();
     return 0;
  } 
  Writer                         = new VS_Writer();
  Writer->bContainer             = true;
  Writer->Reader->OnFrame        = OnFrame;
  Writer->Reader->OnSeek         = OnSeek;
  Writer->Reader->TargetDuration = seg_duration;
  Writer->Reader->PartsInM3U     = HlsParts;
  Writer->EncFps                 = EncFps;
  Writer->BitRate                = BitRate;
  //Writer->Reader->bSeekHard    = true;
  if(bDesktop == true){
     VS_ScreenCarrier *Carrier = new VS_ScreenCarrier;
     Writer->Reader->Prepare(Carrier);
  }else if(bPic == false){
     Writer->AddSource(st_input, VSW_Item::IVIDEO, BeginTime, EndTime-BeginTime);    
  }else{
     Writer->AddSource(st_input, VSW_Item::IFRAME, BeginTime, EndTime-BeginTime);
  }
  if(FileName != NULL)Writer->SetDestination(FileName);
  if(bHls)Writer->out_format = VS_Writer::VS_FRMT_HLS;
  //Writer->SetEncodeSize(OutputWidth, OutputHeight);
  Writer->Start();
 // if(bPic == false){
 //    sleep(10);
 //    Writer->Join();
 // }else{
     for(;;){
         if(bReady || Writer->iEnd == 2)break;
         usleep(100);
     }
  //}


  //delete Writer;
  return 0;
}


