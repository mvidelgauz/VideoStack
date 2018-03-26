//
#ifndef CTHREAD_CLASS 
#define CTHREAD_CLASS 

#include <iostream> 
#include <cstdlib> 
#include <pthread.h> 

class cThread 
{ 
    public: 
          TCPStream   *stream   = NULL;
          int64_t      Wrk       = 0;
          cThread()  {}; 
          ~cThread() {}; 

          bool create_thread(int Type, void *param, void* thread_int(void *prm))
          {
               int ErrNum;

               if ((ErrNum = pthread_create(&pthread_id, NULL, thread_int, this)) != 0)
               {
                    perror("pThread start failed");
                    return false;
               }
               if(Type != 0){
                  stream = (TCPStream *)param;
                  printf("Starting stream sender thread TID:0x%lx\n",pthread_id);
               }else{
                  int *i = (int*)param;
                  Wrk = (int64_t)i;
                  printf("Starting acceptor thread TID:0x%lx\n",pthread_id);
               }
               return true;
          }
          pthread_t     GetTID()
          {
               return pthread_id;
          } 
    private: 
          pthread_t     pthread_id; 
}; 
#endif 
