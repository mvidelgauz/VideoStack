#ifndef _EPOLL_H_
#define _EPOLL_H_

#include <arpa/inet.h>
#include <fcntl.h>              // fcntl
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXLINE 512             // max length of a line
#define MAX_EVENTS 256
#define RESP_HEADER_LENTH 152

// #define TEST_EPOLL

// #define DEBUG
// #define VERBOSE

#ifdef VERBOSE
#ifndef DEBUG
#define DEBUG
#endif
#endif

// sizeof = 168, malloc: 176
typedef struct {
    char    *body_bufptr;          // offset in dict array data
    int     sock_fd;                // file descriptor
    int     body_cnt;               // how remainning byte
    int     is_websocket;           // ==1 it's websocket socket

    char    headers[RESP_HEADER_LENTH];
    char    *headers_bufptr;       // header pointer
    int     headers_cnt;            // header length unwrite
    int     static_fd;
    int     file_cnt;               // unwrite file
    off_t   file_offset;
    void    *stream;
} dict_epoll_data;

int open_nonb_listenfd(int port);
int nonb_write_headers(int fd, char* bufp, int nleft, dict_epoll_data *ptr);
int nonb_write_body(int fd, char* bufp, int nleft, dict_epoll_data *ptr);
int nonb_sendfile(dict_epoll_data *ptr);
void accept_incoming(int listen_sock, int epollfd);
void close_and_clean(dict_epoll_data *ptr, int epollfd);
void enter_loop(int listen_sock, int epollfd);
void process_request(dict_epoll_data *ptr, int epollfd);
void write_response(dict_epoll_data *ptr, int epollfd);

#endif /* _EPOLL_H_ */
