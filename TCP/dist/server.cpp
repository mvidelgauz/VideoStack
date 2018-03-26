//set args 127.0.0.1:1090 127.0.0.1:1091 127.0.0.1:9090  - принимаем наш поток на 1090 и посылаем своим клиентам на 1091 и wbs-клиентам на 9090
//set args -f gt.h264 gt.txt 127.0.0.1:1091 127.0.0.1:9090 - посылаем h264 файл, к которому придается файл с размерам пактов, своим клиентам на 1091 и wbs-клиентам на 9090
//set args -i 127.0.0.1:1090 -hlsv ./hls -fps 25 -parts 6 -sdur 5

#include <stdio.h>
#include <stdlib.h>
#include "tcpacceptor.h"
#include "ctrd.h"
#include <unistd.h> 
#include <fstream>
#include <iostream>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
 #include <sys/types.h>
 #include <sys/wait.h>
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <fcntl.h>
//#include <GL/glx.h>
#include "vSystem/VS_Writer.h"
#include "../WebSrv/web_srv.h"

#define MAX_PATTERN_SIZE 256



void* Worker(void *param);
void   Panic(unsigned int InternalErrCode);
const char *ErrorMessage[] = 
{
    "Unknown",
    "Can\'t create pthread"
};

extern void* (*rdr_alloc)(size_t Size);
extern void (*rdr_free)(void *Buf);

TCPAcceptor           *in_acceptor  = NULL; 
TCPAcceptor           *out_acceptor = NULL;
//TCPAcceptor         *wbs_acceptor = NULL;

TCPStream             *FirstOutStream = NULL;
TCPStream             *InStream = NULL;


char                  *InputAdr      = NULL;
char                  *InputFileName = NULL;
char                  *InputFileSize = NULL;
char                  *WbsAdr        = NULL;
char                  *ClntAdr       = NULL;
char                  *HlsDir        = NULL;
VS_Writer             *Writer        = NULL;
VS_EventCarrier       *Carrier       = NULL;
int                   HlsHdrSent     = false;


uint32_t              Hdr[14*4];
pthread_mutex_t       sCriticalSection;

#define INT_ERR_COUNT sizeof(ErrorMessage)/sizeof(char *)

#define INT_ERR_THREAD 1

void Lock(){
    pthread_mutex_lock(&sCriticalSection);
}

void Unlock(){
    pthread_mutex_unlock(&sCriticalSection);
}

void ProcessWebSocket(dict_epoll_data *ptr){
     TCPStream *Stream = (TCPStream*)ptr->stream;
     Stream->bOpened = true;
     Stream->wbs_receive();
}
void OnWbsConnect(dict_epoll_data *ptr){
     printf("OnWbsConnect");
     TCPStream *Stream = new TCPStream;
     Lock();
     Stream->Next = FirstOutStream;
     FirstOutStream = Stream;
     Stream->bWebSocket = true;
Stream->bOpened = true;
     Stream->dict = ptr;
     Stream->m_sd = ptr->sock_fd;
     ptr->stream = (void*)Stream;
     Unlock();
}
void OnWbsClose(dict_epoll_data *ptr){
     TCPStream *Stream = (TCPStream*)ptr->stream;
     Lock();
     TCPStream *CurStream = FirstOutStream;
     TCPStream *PrevStream = NULL;
     while(CurStream != Stream){
           PrevStream = CurStream;
           CurStream = CurStream->Next;
     }
     if(CurStream != NULL){
        if(Stream == FirstOutStream)FirstOutStream = FirstOutStream->Next;
        else PrevStream->Next = CurStream->Next;
     }
     delete Stream;
     Unlock();
}


void *WorkerSingleCon(void *Parm){
    TCPStream  *Stream;
    uint32_t   Dw[2];
    Stream = (TCPStream*)Parm;
    Stream->receive((uint8_t*)Dw, 8, 0);
}

void *FileThread(void *Parm){
    uint8_t *Buf = NULL;
    int      BufSize = 0;
    FILE *fl = fopen(InputFileSize, "r");
    if(fl == NULL){
       printf("can't read size file\n");
       exit(0);
    }
    fseek(fl, 0L, SEEK_END);
    int FlSize = ftell(fl);
    rewind(fl);
    Buf = (uint8_t*)rdr_alloc(FlSize);
    int sz = fread(Buf, 1, FlSize, (FILE*)fl);
    if(sz != FlSize){
       printf("error of file reading\n");
       exit(0);
    }
    fclose(fl);
    int  ArrSize = 1000;
    int *SizeArr = (int*)rdr_alloc(1000 * sizeof(int));
    int  ArrInd = 0; 
    for(int Off=0;Off<FlSize; Off++){
        SizeArr[ArrInd] = atoi((const char*)Buf+Off);
        while(Buf[Off] != 0xA)Off++;
        //Off++;
        ArrInd++;
        if(ArrInd >= ArrSize){
           ArrSize += 1000;
           int *NewArr = (int*)rdr_alloc(ArrSize * sizeof(int));
           memcpy(NewArr, SizeArr, ArrInd * sizeof(int));
           free(SizeArr);
           SizeArr = NewArr;
        }
    }
    fl = fopen(InputFileName, "r");
    if(fl == NULL){
       printf("can't read input file\n");
       exit(0);
    }
    for(int i=0;i<ArrInd;i++){
        if(SizeArr[i] > BufSize){
           if(Buf != NULL)free(Buf);
           Buf = (uint8_t*)rdr_alloc(SizeArr[i]);
           BufSize = SizeArr[i];
        }
        int sz = fread(Buf, 1, SizeArr[i], (FILE*)fl);
        if(sz != SizeArr[i]){
           printf("error of file reading\n");
           exit(0);
        }
        
        TCPStream *CurStream = FirstOutStream; 
        while(CurStream == 0){
              CurStream = FirstOutStream;
              sleep(1);
        }
        Lock();
        while(CurStream != NULL){
              if(CurStream->bWebSocket == true){
                 printf("sending the packet to wbs_socket: %d\n", SizeArr[i]);
                 CurStream->wbs_send(Buf, SizeArr[i], 0);                  
              }
              CurStream = CurStream->Next;
        }
        Unlock();
        usleep(30);
    }
    fclose(fl);
    if(Buf != NULL)free(Buf);
}


void *OutWorker(void *Parm){
    if (out_acceptor->start() == 0) 
    {
       for (;;) 
        {
            TCPStream *Stream = out_acceptor->accept();
            if (Stream != NULL) 
            {
                Lock();
                Stream->Next = FirstOutStream;
                FirstOutStream = Stream;
                Unlock();
                printf("Accept Connect\n");
                cThread *cth = new cThread();
                if (!cth->create_thread(1, (void *)Stream, WorkerSingleCon))
                {
                    Panic(INT_ERR_THREAD);
                }
                Stream = NULL;
            }
        }
    }else{
        printf("can't start out acceptor\n");
        exit(1);
    }
}
/*void *WbsWorker(void *Parm){
    if (wbs_acceptor->start() == 0) 
    {
       for (;;) 
        {
            TCPStream *Stream = wbs_acceptor->accept();
            if (Stream != NULL) 
            {
                Lock();
                Stream->Next = FirstOutStream;
                FirstOutStream = Stream; 
                Stream->bWebSocket = true;
                Unlock();
                printf("Accept Connect\n");
                cThread *cth = new cThread();
                if (!cth->create_thread(1, (void *)Stream, WorkerSingleCon))
                {
                    Panic(INT_ERR_THREAD);
                }
                Stream = NULL;
            }
        }
    }else{
        printf("can't start out acceptor\n");
        exit(1);
    }
}*/

void *InWorker(void *Parm){
    int32_t    *CurBuf = NULL;
    int        CurSize = 0;
    int        RealSize = 0;
    int32_t    Dw[2];
    int        len;

    len = InStream->receive((uint8_t*)&Hdr[0], 14*4);
    if(len < 1){
       printf("input stream closed or failed\n");
       return 0;
    }
    //if(Writer != NULL){
    //   Writer->Start();
    //}
    for(;;){
        len = InStream->receive((uint8_t*)&Dw[0], 8);
        if(len < 1){
           printf("input stream closed or failed\n");
           return 0;
        }
        if(Dw[0] > RealSize && CurBuf != NULL){
           free(CurBuf);
           CurBuf = NULL;
        }
        if(CurBuf == NULL){
           CurSize   = Dw[0] + 8;
           RealSize  = Dw[0];
           CurBuf    = (int32_t*)rdr_alloc(CurSize);           
        }
        CurBuf[0] = Dw[0];
        CurBuf[1] = Dw[1]; 
        len = InStream->receive(((uint8_t*)CurBuf)+8, Dw[0]);
        if(len < 1 || len != Dw[0]){
           printf("input stream closed or failed\n");
           return 0;
        }
        printf("recieved the packet: %d, size: %d\n", Dw[1], Dw[0]);
        //resending the stream to clients
        Lock();
        if(HlsDir != NULL){
           if(HlsHdrSent == false){
              Carrier->PostHdr((uint8_t*)&Hdr[0], 14*4);
              Writer->Start();
              HlsHdrSent = true;
           }
           Carrier->Post((uint8_t*)CurBuf, ((uint8_t*)CurBuf)+8*4, Dw[0]-6*4);
        }
        TCPStream *CurStream = FirstOutStream;
        while(CurStream != NULL){
              if(CurStream->bWebSocket == false){
                 if(CurStream->bHdrSent == false){
                    len = CurStream->send((uint8_t*)&Hdr[0], 14*4);
                    if(len == 14*4)CurStream->bHdrSent = true;
                 }              
                 if(CurStream->bHdrSent == true){
                    CurStream->send((uint8_t*)CurBuf, CurSize);
                 } 
              }
              CurStream = CurStream->Next;
        }

        CurStream = FirstOutStream;
        while(CurStream != NULL){
              if(CurStream->bWebSocket == true){
                 printf("sending the packet to wbs_socket: %d\n", Dw[0]-6*4);
                 CurStream->wbs_send(((uint8_t*)CurBuf)+8*4, Dw[0]-6*4, 0);                  
              }
              CurStream = CurStream->Next;
        }
        Unlock();
    } 
    return 0;
}

void *InThread(void *Parm){
    if (in_acceptor->start() == 0) 
    {
       for (;;) 
        {
            InStream = in_acceptor->accept();
            if (InStream != NULL) 
            {
                printf("Accept Connect\n");
                InWorker(NULL);   //we can handle one input stream only
                InStream = NULL;
            }
        }
    }

}

void PrintHelp(){
    printf("Use: mc_server -[i|f] input -[wbs|hls|clnt] output\n"
           "-i {address:port} port to listen the input stream\n"
           "-f {input file input_file_sizes} use the file as the input stream\n"
           "-wbs {address:port} send the input as the web-socket stream to port clients\n"
           "-clnt {address:port} send the input as the out stream to port clients\n"
           "-hlsv {directory} generate the hls-stream files from the input and save them to a directory\n"
           "-fps - frame per second for output hls\n"
           "-parts {number of existing segments in m3u file}\n"
           "-sdur {sec.ms} - hls segment duration\n"          
           "-btr {output bitrate}\n"
     );
	
}

int init_web_srv(int port);
int 
main(
    int argc, 
    char** argv)
{
    cThread     *cth          = NULL;
    double   seg_duration  = 10.0;
    int      bHls          = false;
    int      bWasParts     = 0;
    int      HlsParts      = 0;
    double   EndTime       = 0;
    int      EncFps        = 30;
    int      BitRate       = 40000;
    if(argc < 5){
       PrintHelp();
       exit(0);
    }
    for(int i=1; i<argc; i++){
        if(strcmp(argv[i], "-i") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             InputAdr = argv[i+1];
             i++;
        }else if(strcmp(argv[i], "-f") == 0){
             if(i + 1 == argc || i + 2 == argc){
                PrintHelp();
                exit(0);
             }
             InputFileName = argv[i+1];
             InputFileSize = argv[i+2];
             i += 2;
        }else if(strcmp(argv[i], "-wbs") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             WbsAdr = argv[i+1];
             i++;
        }else if(strcmp(argv[i], "-clnt") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             ClntAdr = argv[i+1];
             i++;
        }else if(strcmp(argv[i], "-hlsv") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             HlsDir = argv[i+1];
             if(bWasParts == 0)HlsParts = 6;
             i++;
        }else if(strcmp(argv[i], "-parts") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);                
             }
             HlsParts = strtol(argv[i+1], NULL, 10);
             bWasParts = 1;
             i++;
             continue;
        }else if(strcmp(argv[i], "-sdur") == 0){
             if(i + 1 == argc){
                PrintHelp();
                exit(0);
             }
             seg_duration = atof(argv[i+1]);
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
         }
     }//for(int i=1; i<argc; i++)
     if((InputAdr != NULL && InputFileName != NULL) || (InputAdr == NULL && InputFileName == NULL)){
         printf("Incorrect input stream\n");
         exit(0);
     }
     pthread_mutex_init(&sCriticalSection, NULL);

     if(HlsDir != NULL){
        Writer                         = new VS_Writer();
        Writer->bContainer             = true;
        //Writer->Reader->OnFrame        = OnFrame;
        //Writer->Reader->OnSeek         = OnSeek;
        Writer->Reader->TargetDuration = seg_duration;
        //Writer->Reader->PartsInM3U     = HlsParts;
        Writer->EncFps                 = EncFps;
        Writer->BitRate                = BitRate;
        Writer->SetDestination(HlsDir);
        //Writer->Reader->bSeekHard      = true;
        Carrier                        = new VS_EventCarrier;
        Writer->Reader->Prepare(Carrier, 3);
        Writer->out_format             = VS_Writer::VS_FRMT_HLS;
        
     }
     
     if(InputFileName != NULL){
        cth = new cThread();
        if(!cth->create_thread(0, (void*)5, FileThread))
           Panic(INT_ERR_THREAD);     
        exit(0);
    }

    if(InputAdr != NULL){
       int port;
       //we send the stream from the source app to this address
       char *pst2 = strchr(InputAdr, ':');
       if(pst2 != NULL){
          pst2[0] = 0;
          pst2++;
          sscanf(pst2, "%d", &port);           
       }
       in_acceptor = new TCPAcceptor(port, InputAdr);
       cth = new cThread();
       if(!cth->create_thread(0, (void*)4, InThread))
           Panic(INT_ERR_THREAD); 
    }


    //we give out this stream in our container format, to mc_player, to check that the stream is ok
    if(ClntAdr != NULL){
       int port;
       char *pst2 = strchr(ClntAdr, ':');
       if(pst2 != NULL){
          pst2[0] = 0;
          pst2++;
          sscanf(pst2, "%d", &port);           
       }

       out_acceptor = new TCPAcceptor(port, ClntAdr);
       cth = new cThread();
       if(!cth->create_thread(0, (void*)1, OutWorker))
           Panic(INT_ERR_THREAD);
    }
 
                
    //we give out this stream to any one who is connecting to our web-socket
    if(WbsAdr != NULL){
       int  port;
       char *pst2 = strchr(WbsAdr, ':');
       if(pst2 != NULL){
          pst2[0] = 0;
          pst2++;
          sscanf(pst2, "%d", &port);           
       }
      
       init_web_srv(port);    
    }

    for(;;){
        sleep(1);
    }
}


void
Panic(
   unsigned int  InternalErrCode)
{
   if (InternalErrCode > INT_ERR_COUNT)
   {
       printf("Internal problem with number\n");
       fflush(stdout);
       exit(-1);
   }
   printf("The problem is detected - %s\n", ErrorMessage[InternalErrCode]);
   fflush(stdout);
   exit(InternalErrCode);
} 
