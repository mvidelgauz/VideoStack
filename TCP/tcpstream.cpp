#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
//#include "tcpstream.h"
#include "tcpacceptor.h"


TCPStream::TCPStream(int sd, struct sockaddr_in* address) : m_sd(sd) {
    char ip[50];
    inet_ntop(PF_INET, (struct in_addr*)&(address->sin_addr.s_addr), ip, sizeof(ip)-1);
    m_peerIP = ip;
    m_peerPort = ntohs(address->sin_port);
    Prev = NULL; Next = NULL; bWebSocket = false; bHdrSent = false;
}

TCPStream::~TCPStream()
{
    close(m_sd);
}

int TCPStream::send(uint8_t* pBuf, size_t Size) 
{
    int Len = Size;

    while(Len > 0){             
          int bread = ::send(m_sd, pBuf, Len, MSG_NOSIGNAL);        
          if(bread == -1){
             return -1;
          }
          pBuf += bread;
          Len -= bread;
    }//while(len > 0) 
    return Size-Len;
}

ssize_t TCPStream::receive(uint8_t* pBuf, size_t Size) 
{
    int Len = Size;

    while(Len > 0){             
          int bread = ::recv(m_sd, pBuf, Len, MSG_NOSIGNAL);        
          if(bread == -1){
             return -1;
          }
          pBuf += bread;
          Len -= bread;
    }//while(len > 0) 
    return Size-Len;
}

/*ssize_t TCPStream::receive(uint8_t* buffer, size_t len, int timeout) 
{
    if (timeout <= 0) return read(m_sd, buffer, len);

    if (waitForReadEvent(timeout) == true)
    {
        return read(m_sd, buffer, len);
    }
    return connectionTimedOut;

}*/
void TCPStream::wbs_receive(){
    uint8_t SmallBuf[8];
    int d = receive(SmallBuf, 2);
    if(d == 0 || d == -1){    
       return;  
    }
    int Z = SmallBuf[0];  //opcode and flags
    int A = SmallBuf[1];
    int PLen;   //payload length   
    int Masked;
    int SizeLen;
    if(A > 127){
       Masked = 1;
       A -= 128;
    }
     
    if(A == 126){
       d = receive(SmallBuf, 2);
       if(d == 0 || d == -1){        
          return;  
       }
       uint16_t *wBuf = (uint16_t*)SmallBuf;
       PLen = wBuf[0];
       PLen = ntohs(PLen);
       SizeLen = 2;
    }else if(A == 127){
       d = receive(SmallBuf, 8);
       if(d == 0 || d == -1){        
          return;
       }
       uint64_t *qBuf = (uint64_t*)SmallBuf;
       uint64_t qLen = qBuf[0];
       PLen = ntohl(qLen);
       SizeLen = 8;
    }else{
       uint32_t *Bf;
       PLen = A;
       SizeLen = 1;
    }
    if(Masked == 1){
       d = receive(SmallBuf, 4);
       if(d == 0 || d == -1){    
          return;  
       }              
    }
 
    int q = PLen;
    uint8_t *pBuf2 = (uint8_t*)malloc(q+1);
    d = receive(pBuf2, q);
    if(d == 0 || d == -1){
       free(pBuf2);
       return;
    }
    if(Masked == 1){
       for(int i = 0; i < PLen; i++) {
           int Md = i % 4;
           int Xor = SmallBuf[Md];
           pBuf2[i] = pBuf2[i] ^ Xor;
       }
    }//if(Masked == 1)
    pBuf2[q] = 0;
 printf("wbs_receive: %s\n", pBuf2);
}
int TCPStream::wbs_send(uint8_t *pBuf, int Size, int bMask){
    int rt = Size;
    int A = 2;  //for text should be 1
 
//bMask = 0;
    A = 0x80 + A;  //1 is final bit, if the message is too long, it can be partial
    int B;
    uint8_t *Hdr = (uint8_t*)malloc(20);
    Hdr[0] = A;
 
    uint32_t *Dwr;    //the pointer to the mask
    int   Len;       //the length of the header
    int   MaskLen = 0;
    if(bMask == true)MaskLen = 4;
    if(rt < 126){
       if(bMask == true)B = (rt + 0x80);
       else B = rt;
       Hdr[1] = B;
       Dwr = (uint32_t*)(Hdr + 2);
       Len = 2 + MaskLen;
    }else if(rt <= 0xFFFF){
       if(bMask == true)B = 126 + 0x80;
       else B = 126;
       Hdr[1] = B;
       int16_t *Wrd = (int16_t*)(Hdr + 2);
       int PLen = ntohs(rt);
       Wrd[0] = PLen;
       Dwr = (uint32_t*)(Hdr + 4);
       Len = 2 + 2 + MaskLen;
    }else{
 
       if(bMask == true)B = 127 + 0x80;
       else B = 127;
       Hdr[1] = B;
       uint64_t *Qrd = (uint64_t*)(Hdr + 2);   
       uint64_t PLen = htobe64(rt);
       Qrd[0] = PLen;    
       Dwr = (uint32_t*)(Hdr + 2 + 8);
       Len = 10 + MaskLen;
    }
    if(bMask == true){
      int Mask = rand();
      Dwr[0] = Mask;
    }
    send(Hdr, Len);

    if(bMask == true){
       uint8_t  *binMask = (uint8_t*)Dwr;
       for(int i = 0; i < rt; i++) {
           int Md = i % 4;
           int Xor = binMask[Md];
           int Bs = pBuf[i];
           pBuf[i] = Bs ^ Xor;
       }
    }
    send(pBuf, rt);
    free(Hdr); 
    return 1;
}
string TCPStream::getPeerIP() 
{
    return m_peerIP;
}

int TCPStream::getPeerPort() 
{
    return m_peerPort;
}

bool TCPStream::waitForReadEvent(int timeout)
{
    fd_set sdset;
    struct timeval tv;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    FD_ZERO(&sdset);
    FD_SET(m_sd, &sdset);
    if (select(m_sd+1, &sdset, NULL, NULL, &tv) > 0)
    {
        return true;
    }
    return false;
}
