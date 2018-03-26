#include "rio.h"
#include "web_srv.h"
#include "search.h"
#include "network.h"
#include <ctype.h>
#include <iostream>
#include "static.h"
#include "../sha1/sha1.hpp"

#define GZIP_MAGIC 0x8b1f
static char gzip_header[] = {
    (char) GZIP_MAGIC,          // Magic number (short)
    (char) (GZIP_MAGIC >> 8),   // Magic number (short)
    8,                    // Compression method (CM) Deflater.DEFLATED
    0,                    // Flags (FLG)
    0,                    // Modification time MTIME (int)
    0,                    // Modification time MTIME (int)
    0,                    // Modification time MTIME (int)
    0,                    // Modification time MTIME (int)
    0,                    // Extra flags (XFLG)
    0                     // Operating system (OS)
};

static char *json_headers        = "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\nCache-Control: max-age=86400, public\r\nContent-Type: application/json\r\n\r\n";
static char *gziped_json_headers = "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\nCache-Control: max-age=86400, public\r\nContent-Encoding: gzip\r\nContent-Type: application/json\r\n\r\n";
static char *websocket_answer    = "HTTP/1.1 101 Switching Protocols\r\n" 
                                   //"Server: nightshade video server\r\n" 
                                   "Upgrade: websocket\r\n" 
					               "Connection: Upgrade\r\n"
                                   "Sec-WebSocket-Accept: %s\r\n\r\n";
					               //"Sec-WebSocket-Accept: %s\r\n"  
                                   //"Sec-WebSocket-Protocol: binary\r\n\r\n";
#ifdef SINGLE
void ProcessWebSocket(dict_epoll_data *ptr){
     printf(" ProcessWebSocket\r\n");
}
void OnWbsConnect(dict_epoll_data *ptr){
     printf(" OnWbsConnect\r\n");
}
void OnWbsClose(dict_epoll_data *ptr){
     printf(" OnWbsClose\r\n");
}
#else
void ProcessWebSocket(dict_epoll_data *ptr);
void OnWbsConnect(dict_epoll_data *ptr);
void OnWbsClose(dict_epoll_data *ptr);
#endif


/*
 * encodeblock
 * кодирует 3 8-битных байта как 4 6-ти битных символа
*/
char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void encodeblock( unsigned char in[3], unsigned char out[4], int len )
{

	/*
	 * Таблица преобразований согласно RFC1113
	*/
	

    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

/*
 * encode
 *
 * base64 кодирование. Кодирует входной буфер buf длиной linesize
 * выходной буфер записывается в outBuf
*/
void EncodeBase64( char *buf, int linesize, char * outBuf, int bSlash)
{
    unsigned char in[3], out[4];
    int i, len, outPos = 0;

    for(int pos = 0; pos <= linesize; ){
        len = 0;
        for( i = 0; i < 3; i++,pos++ ) {
			//printf("1:%c[%d]\n",buf[pos],buf[pos]);
			if(buf[pos] == '\\' && buf[pos+1] == '0' && bSlash == true) {
				pos++;
				in[i]=0;
			} else {
				in[i] = (unsigned char) buf[pos];
			}

            if( pos  < linesize ) {
                len++;
            }
            else {
                in[i] = 0;
            }
        }
        if( len ) {
            encodeblock( in, out, len );
            for( i = 0; i < 4; i++ ) {
                //printf("%c",out[i]);
				outBuf[outPos] = out[i];
				outPos++;
            }
        }
    }
}

static inline bool is_base64(unsigned char c) {
     return (isalnum(c) || (c == '+') || (c == '/'));
}
int DecodeBase64(char *encoded_string, int in_len, char * outBuf) {
     int i = 0;
     int j = 0;
     int in_ = 0;
     unsigned char char_array_4[4], char_array_3[3];
     int i2;
	 char *pst1;

	 char Smbl[2] = {NULL};
     for(i2=0; ;){
	     if(in_len == 0 || encoded_string[in_] == '=' || is_base64(encoded_string[in_]) == false)break;
         char_array_4[i++] = encoded_string[in_]; in_++;
         if(i ==4 ){
	        for(i = 0; i <4; i++){
				pst1 = (char*)cb64;
				Smbl[0] = char_array_4[i];
	            //if(PrsMain.SymFind(&pst1, Smbl) == false)return i2;
                int k;
                for(k=0; ;k++){
                    if(pst1[k] == 0)return i2;
                    if(Smbl[0] == pst1[k])break;
                }
	            //char_array_4[i] = (DWORD)pst1 - (DWORD)cb64;
                char_array_4[i] = k;       
			}

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

             for(i = 0; i < 3; i++){
                 outBuf[i2] = char_array_3[i];
			     i2++;				
			 }//for(i = 0; i <4; i++){
             i = 0;
          }//if(i ==4)
      }//for(i2=0; ;i2++)

      if(i){
         for(j = i; j <i; j++)
             char_array_4[j] = 0;

         for(j = 0; j <i; j++){
			 pst1 = (char*)cb64;
	         Smbl[0] = char_array_4[j];
	         //if(PrsMain.SymFind(&pst1, Smbl) == false)return i2;
	         //char_array_4[j] = (uint32_t)pst1 - (uint32_t)cb64;
             int k;
             for(k=0; ;k++){
                 if(pst1[k] == 0)return i2;
                  if(Smbl[0] == pst1[k])break;
             }           
             char_array_4[j] = k;    
		 }             

         char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
         char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
         char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

         for(j = 0; (j < i - 1); j++){
			 outBuf[i2] = char_array_3[j];
			 i2++;
		 }
     }
	 outBuf[i2] = 0;
     return i2;
}



int nonb_write_headers(int fd, char* bufp, int nleft, dict_epoll_data *ptr) {
    int nwritten;
    while(nleft > 0) {
        if((nwritten= write(fd, bufp, nleft)) <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ptr->headers_cnt = nleft;
                ptr->headers_bufptr = bufp;
            }
            return 0;           // blocked, do not continue
        }
#ifdef VERBOSE
        printf("write headers count %d, sock_fd %d\n", nwritten, fd);
#endif
        nleft -= nwritten;
        bufp += nwritten;
    }
    ptr->headers_cnt = 0;       // done all
    return 1;
}

int nonb_sendfile(dict_epoll_data *ptr) {
    int nsent;
    off_t offset = ptr->file_offset;
    while (ptr->file_cnt) {
        nsent = sendfile(ptr->sock_fd, ptr->static_fd, &offset, ptr->file_cnt);
        if(nsent > 0) {
            ptr->file_cnt -= nsent;
            ptr->file_offset = offset; //  save for next call
        } else {
#ifdef DEBUG
            printf("sendfile return early: %d\n", nsent);
#endif
            return 0;
        }
#ifdef DEBUG
        printf("sendfile sock_fd: %d, file_fd: %d, bytes: %d\n",
               ptr->sock_fd, ptr->static_fd, nsent);
#endif
    }
    if (ptr->static_fd) {       // finish send file
        close(ptr->static_fd);
        close(ptr->sock_fd);              //  do not keep-alive
        ptr->static_fd = 0;
    }
    return 1;
}

int nonb_write_body(int fd, char* bufp, int nleft, dict_epoll_data *ptr) {
    int nwritten;
    while(nleft > 0) {
        if((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ptr->body_bufptr = bufp;
                ptr->body_cnt = nleft;
            }            return 0;           // blocked, do not continue
        }
#ifdef VERBOSE
        printf("write body count %d, fd %d\n", nwritten, fd);
#endif
        nleft -= nwritten;
        bufp += nwritten;
    }
    close(fd);                  // do not keep-alive
    ptr->body_cnt = 0;          // done all
    return 1;
}

int wbs_write_req(int fd, char* bufp, int nleft, dict_epoll_data *ptr) {
    int nwritten;
    while(nleft > 0) {
        if((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }            
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return 1;
}

void accept_incoming(int listen_sock, int epollfd) {
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof clientaddr;
    int conn_sock = accept4(listen_sock, (SA *)&clientaddr, &clientlen, SOCK_NONBLOCK);
    if (conn_sock <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // printf("all connection\n"); //  we have done all
        } else {
            perror("accept");
        }
    } else {
#ifdef DEBUG
        printf("accept %s:%d, sock_fd is %d\n", inet_ntoa(clientaddr.sin_addr),
               ntohs(clientaddr.sin_port), conn_sock);
#endif
        // make_socket_non_blokcing(conn_sock);
        struct epoll_event ev;
        dict_epoll_data *data = (dict_epoll_data*)malloc(sizeof(dict_epoll_data));
        data->sock_fd = conn_sock;
        data->body_cnt = 0;     // init, default value
        data->headers_cnt = 0;
        data->is_websocket = 0;
        data->file_cnt = 0;
        data->static_fd = 0;
        ev.data.ptr = data;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP| EPOLLET; //  read, edge triggered
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
            perror("epoll_ctl: for read");
            exit(EXIT_FAILURE);
        }
    }
}

void close_and_clean(dict_epoll_data *ptr, int epollfd) {
    // closing a file descriptor cause it to be removed from all epoll sets automatically
    // epoll_ctl(epollfd, EPOLL_CTL_DEL, ptr->sock_fd, NULL);
    close(ptr->sock_fd);
    if (ptr->static_fd) {
        close(ptr->static_fd);
    }
    free(ptr);
}

void handle_request(dict_epoll_data *ptr, char uri[]) {
    //int uri_length = strlen(uri);
    /*if (uri_length > 3 && uri[0] == '/' && uri[1] == 'd' && uri[2] == '/') {
        char *loc = search_word(uri + 3); // 3 is /d/:word
        if (loc) {
            int data_size = read_short(loc, 0);
            int header_length = 0;
            char *headers = ptr->headers;
            if (data_size > 0xe000) { // first bit: is not gzipped
                data_size -= 0xe000;
                sprintf(headers, json_headers, data_size);
                header_length = strlen(headers);
            } else {
                sprintf(headers, gziped_json_headers, data_size + 10);
                header_length = strlen(headers);
                // copy gzip_header with http headers
                memcpy(headers + header_length, gzip_header, 10);
                header_length += 10;
            }
            int cont = nonb_write_headers(ptr->sock_fd, headers,
                                          header_length, ptr);
            if (cont) {
                // 2 byte for size, 1 byte for zipped or not
                nonb_write_body(ptr->sock_fd, loc + 2, data_size, ptr);
            } else {
                ptr->body_bufptr = loc;
                ptr->body_cnt = data_size;
            }
        } else {
            client_error(ptr->sock_fd, 404, "Not found", "");
        }
    } else {*/
        uri = uri + 1;
        int uri_length = strlen(uri);
        if (uri_length == 0) {
#ifdef PRODUCTION
            uri = "dict.html.gz";
#endif
#ifndef PRODUCTION
            uri = "index.html";
#endif
        }
#ifdef PRODUCTION
        else if (uri[uri_length-2] == 'j' && uri[uri_length-1] == 's') {
            strcat(uri, ".gz"); // use gziped js, uri is large enough
        }
#endif
#ifdef DEBUG
        printf("sock_fd %d, request file %s\n", ptr->sock_fd, uri);
#endif
        serve_file(ptr, uri);
    //}
}

int TranslateToBin(char *Src, uint8_t *Dest){
    int i2 = 0;
    for(int i=0; ; i+=2){
        if(Src[i] == 0)break;
        if(Src[i] <= 0x39)Dest[i2] = (Src[i] - 0x30) << 4;
        else if(Src[i] < 0x61)Dest[i2] = (Src[i] - 0x41 + 10) << 4;
        else Dest[i2] = (Src[i] - 0x61 + 10) << 4;

        if(Src[i+1] <= 0x39)Dest[i2] += (Src[i+1] - 0x30);
        else if(Src[i+1] < 0x61)Dest[i2] += (Src[i+1] - 0x41 + 10);
        else Dest[i2] += (Src[i+1] - 0x61 + 10);
        i2++;
    }
    return i2;
}
void process_request(dict_epoll_data *ptr, int epollfd) {
    rio_t rio;
    char *WbsKey = NULL; 
    int  WbsKeyLen;
    char buf[MAXLINE], method[16], uri[MAXLINE];
    rio_readinitb(&rio, ptr->sock_fd);
    int c = rio_readlineb(&rio, buf, MAXLINE);
    if (c > 0) {
        sscanf(buf, "%s %s", method, uri); // http version is not cared
#ifdef DEBUG
        printf("method: %s, uri: %s, count: %d, line: %s", method, uri, c, buf);
#endif
        // read all
        while(buf[0] != '\n' && buf[1] != '\n') { // \n || \r\n
            c = rio_readlineb(&rio, buf, MAXLINE);
            if (c == -1) { break; }
            else if (c == 0 ) { // EOF, remote close conn
#ifdef DEBUG
                printf("reading header: clean, close sock_fd %d\n", ptr->sock_fd); //
#endif
                close_and_clean(ptr, epollfd);
                return;
            }
            if(strncmp(buf, "Upgrade:", 8) == 0){
               char *pst1 = buf + 8;
               while(pst1[0] == 0x20)pst1++;
               if(strncmp(pst1, "websocket", 9) == 0){
                  printf("Upgrading to web-sockets, sending web-socket answer\n");
                  ptr->is_websocket = 1;                  
               }
            }
            if(strncmp(buf, "Sec-WebSocket-Key:", 18) == 0){
               char *pst1 = buf + 18;
               while(pst1[0] == 0x20)pst1++;
               char *pst2 = pst1;
               while(pst2[0] != 0x0D)pst2++;
               WbsKeyLen = pst2 - pst1;
               WbsKey = (char*)malloc(WbsKeyLen + 1);
               memcpy(WbsKey, pst1, WbsKeyLen);
               WbsKey[WbsKeyLen] = 0;
            }
printf("%s\n", buf);
        }
        if(ptr->is_websocket == 1){
           char *wbs_response = (char*)malloc(1024);
           char *Hd = (char*)malloc(WbsKeyLen+40);  
           //DecodeBase64(WbsKey, WbsKeyLen, Hd);
           memcpy(Hd, WbsKey, WbsKeyLen);
           strcpy(Hd+WbsKeyLen, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
           SHA1 checksum;
           std::string input = Hd;
           checksum.update(input);
           const std::string Hash = checksum.final();
           char *szHash = (char*)Hash.c_str();
           uint8_t *BinHash = (uint8_t*)malloc(Hash.length());
           int rt2 = TranslateToBin(szHash, BinHash);
	       char *Hash64 = (char*)malloc(Hash.length() * 2);
           EncodeBase64((char*)BinHash, rt2, Hash64, true);
           int rt = snprintf(wbs_response, 1024, websocket_answer, Hash64);
           wbs_write_req(ptr->sock_fd, wbs_response, rt, ptr);
           OnWbsConnect(ptr);
           free(Hd);
           free(wbs_response);
           return;
        }
        url_decode(uri, buf, MAXLINE);
        handle_request(ptr, buf); // reuse buffer
    } else if (c == 0) {       // EOF, remote close conn
#ifdef DEBUG
        printf("reading line: clean, close sock_fd %d\n", ptr->sock_fd);
#endif
        close_and_clean(ptr, epollfd);
    }
}

void write_response(dict_epoll_data *ptr, int epollfd) {
    if (ptr->headers_cnt) {
        char *bufp = ptr->headers_bufptr;
        int cont = nonb_write_headers(ptr->sock_fd, bufp, strlen(bufp), ptr);
        if (cont) {
            bufp = ptr->body_bufptr;
            nonb_write_body(ptr->sock_fd, bufp, strlen(bufp), ptr);
        }
    } else if (ptr->body_cnt) {
        char *bufp = ptr->body_bufptr;
        nonb_write_body(ptr->sock_fd, bufp, strlen(bufp), ptr);
    }
    if (ptr->file_cnt) {
        nonb_sendfile(ptr);
    }
}

void enter_loop(int listen_sock, int epollfd) {
    int nfds;
    uint32_t events;
    struct epoll_event epoll_events[MAX_EVENTS];
    while(1) {
        nfds = epoll_wait(epollfd, epoll_events, MAX_EVENTS, -1);
#ifdef VERBOSE
        printf("epoll wait return %d events\n", nfds);
#endif
        for (int i = 0; i < nfds; ++ i) {
            events = epoll_events[i].events;
            if (epoll_events[i].data.fd == listen_sock) {
                accept_incoming(listen_sock, epollfd);
            }  else {
                dict_epoll_data *ptr = (dict_epoll_data*) epoll_events[i].data.ptr;
                if ((events & EPOLLRDHUP) || (events & EPOLLERR)
                    || (events & EPOLLRDHUP)) {
#ifdef DEBUG
                    printf("error condiction, events: %d, fd: %d\n",
                           events, ptr->sock_fd);
#endif
                    if(ptr->is_websocket == 1)OnWbsClose(ptr);
                    close_and_clean(ptr, epollfd);
                } else {
                    if (events & EPOLLIN) {
#ifdef DEBUG
                        printf("process request, sock_fd %d\n", ptr->sock_fd);
#endif
                        if(ptr->is_websocket == 1)ProcessWebSocket(ptr);
                        else process_request(ptr, epollfd);
                    }
                    if (events & EPOLLOUT & ptr->is_websocket == 0) {
                        write_response(ptr, epollfd);
                    }
                }
            }
        }
    }
}
#ifdef SINGLE
int main(int argc, char** argv) {
    port = 9090;
#else
int init_web_srv(int port){
#endif
    struct epoll_event ev;
    int listen_sock, efd;

    listen_sock = open_nonb_listenfd(port);
    efd = epoll_create1(0);
    if (efd == -1) { perror("epoll_create"); exit(EXIT_FAILURE); }

    ev.events = EPOLLIN;        // read
    ev.data.fd = listen_sock;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
    //init_dict_search();
    enter_loop(listen_sock, efd);
    return 0;
}
