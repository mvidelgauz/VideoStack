#ifndef __tcpstream_h__
#define __tcpstream_h__

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <iostream> 
#include <cstdlib> 
#include <stdio.h>
#include <stdlib.h>
#include "../WebSrv/web_srv.h"

using namespace std;

class TCPStream
{
public:
    int             m_sd;
    string          m_peerIP;
    int             m_peerPort;

  
    friend class    TCPAcceptor;
    friend class    TCPConnector;
    friend class    cThread;

    ~TCPStream();

    int             wbs_send(uint8_t *pBuf, int Size, int bMask);
    void            wbs_receive();
    int             send(uint8_t* buffer, size_t len);
    ssize_t         receive(uint8_t* buffer, size_t len, int timeout=0);

    string          getPeerIP();
    int             getPeerPort();

    TCPStream       *Prev;
    TCPStream       *Next;
    int             bWebSocket;
    int             bHdrSent;
    int             bOpened;   //websocket is opened for sending video
    dict_epoll_data *dict;

    enum {
        connectionClosed = 0,
        connectionReset = -1,
        connectionTimedOut = -2
    };


    bool            waitForReadEvent(int timeout);
    void            *Worker(void *param);
    
    TCPStream(int sd, struct sockaddr_in* address);
    TCPStream(){Prev = NULL; Next = NULL; bWebSocket = false; bHdrSent = false;};;
    TCPStream(const TCPStream& stream){Prev = NULL; Next = NULL; bWebSocket = false; bHdrSent = false;};;
};

#endif
