/* toralize.h */
#ifndef TORALIZE_H
#define TORALIZE_H

#include <stdio.h>
#include <stdlib.h> // for atoi
#include <string.h>
#include <unistd.h> // first 4 r the "necessary" libs
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dlfcn.h>

#define PROXYIP       "127.0.0.1"
#define PROXYPORT     9050  /* Default Tor SOCKS port */ 
#define USERNAME      "toraliz"
#define reqsize sizeof(struct proxy_request)
#define ressize sizeof(struct proxy_response)

typedef unsigned char int8;
typedef unsigned short int int16;
typedef unsigned int int32;

// https://www.rfc-editor.org/rfc/rfc1928
// There is a 1 byte field of vn numbr (should be 4). 1 byte field for cd (shoukd be 1 for CONNECT. CD is the command code. Can be BIND or CONNECT). 2 byte field for DSTPORT2 (Destination port number, needs to be formated in network byte order). 4 byte field for DSTIP (Destination ip, this is where the ip address goes). Variable field byte for USERID (Name - will be toraizer). 1 Byte for null field (should be 0).
//
typedef struct proxy_request {
    int8 vn;
    int8 cd;
    int16 dstport;
    int32 dstip;
    unsigned char userid[8];
} Req;

//typedef struct proxy_request Req;

// VN is the same always. CD is the only important call. We still need to call everything. 

typedef struct proxy_response {
    int8 vn;
    int8 cd;
    int16 _; //DSPORT 
    int32 __; //destip
} Res;

//typedef struct proxy_response Res;

//int perform_socks5_handshake(int sockfd, const struct sockaddr *dest_addr, socklen_t addrlen);

Req *request(struct sockaddr_in *);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

#endif /* TORALIZE_H */

