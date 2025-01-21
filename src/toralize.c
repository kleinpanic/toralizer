#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

// Configure your local Tor SOCKS proxy here:
#define TOR_PROXY_IP   "127.0.0.1"
#define TOR_PROXY_PORT 9050

// Minimal buffer sizes for the SOCKS5 handshake.
#define SOCKS5_BUFSZ   256

// Real connect() pointer, resolved via dlsym.
static int (*real_connect)(int, const struct sockaddr*, socklen_t) = NULL;

// Helper: send() all bytes until done or error.
static ssize_t full_send(int fd, const void *buf, size_t len) {
    size_t total = 0;
    const char* ptr = buf;
    while (total < len) {
        ssize_t rc = send(fd, ptr + total, len - total, 0);
        if (rc < 0) {
            return -1;
        }
        total += rc;
    }
    return total;
}

// Helper: recv() exactly len bytes or error.
// In real code, consider timeouts/partial reads carefully.
static ssize_t full_recv(int fd, void *buf, size_t len) {
    size_t total = 0;
    char* ptr = buf;
    while (total < len) {
        ssize_t rc = recv(fd, ptr + total, len - total, 0);
        if (rc <= 0) {
            return -1;
        }
        total += rc;
    }
    return total;
}

/**
 * Perform a SOCKS5 handshake on proxy_fd to connect to the given target.
 * We handle both IPv4 and IPv6 by checking 'family'.
 *
 * Returns 0 on success, -1 on failure.
 */
static int socks5_handshake(int proxy_fd, const struct sockaddr *target_addr)
{
    unsigned char buf[SOCKS5_BUFSZ];

    // 1) Send greeting: [ VERSION=5, NMETHODS=1, METHOD=0x00 (no-auth) ]
    unsigned char greeting[3] = {0x05, 0x01, 0x00};
    if (full_send(proxy_fd, greeting, sizeof(greeting)) < 0) {
        perror("send(greeting)");
        return -1;
    }

    // 2) Receive server's choice: 2 bytes: [0x05, method]
    if (full_recv(proxy_fd, buf, 2) < 0) {
        perror("recv(greeting-response)");
        return -1;
    }
    if (buf[0] != 0x05 || buf[1] != 0x00) {
        fprintf(stderr, "SOCKS5 handshake: server chose unsupported method 0x%02x.\n",
                buf[1]);
        return -1;
    }

    // 3) Build the CONNECT request:
    //    [ VER=0x05 | CMD=0x01 | RSV=0x00 | ATYP=? | DST.ADDR | DST.PORT ]
    // For IPv4: ATYP=0x01 + 4-byte IP, for IPv6: ATYP=0x04 + 16-byte IP.
    //
    // We'll store it in buf[] for convenience.
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x05;  // version
    buf[1] = 0x01;  // cmd = CONNECT
    buf[2] = 0x00;  // reserved

    int req_len = 0;  // how many bytes to send total

    if (target_addr->sa_family == AF_INET) {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)target_addr;
        buf[3] = 0x01;  // ATYP = IPv4
        memcpy(&buf[4], &addr4->sin_addr.s_addr, 4);    // 4-byte IPv4
        memcpy(&buf[8], &addr4->sin_port, 2);           // 2-byte port (network order)
        req_len = 10;
    }
    else if (target_addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)target_addr;
        buf[3] = 0x04;  // ATYP = IPv6
        memcpy(&buf[4], &addr6->sin6_addr, 16);         // 16-byte IPv6
        memcpy(&buf[20], &addr6->sin6_port, 2);         // 2-byte port
        req_len = 22;
    }
    else {
        fprintf(stderr, "socks5_handshake: unsupported family %d\n", target_addr->sa_family);
        return -1;
    }

    // 4) Send the CONNECT request
    if (full_send(proxy_fd, buf, req_len) < 0) {
        perror("send(connect-request)");
        return -1;
    }

    // 5) Read the first 4 bytes of server's reply
    //    [ VER=0x05 | REP=? | RSV=0x00 | ATYP=? ]
    if (full_recv(proxy_fd, buf, 4) < 0) {
        perror("recv(connect-reply-head)");
        return -1;
    }
    if (buf[0] != 0x05) {
        fprintf(stderr, "SOCKS5 reply: invalid version 0x%02x\n", buf[0]);
        return -1;
    }
    if (buf[1] != 0x00) {
        fprintf(stderr, "SOCKS5 CONNECT failed, REP=0x%02x\n", buf[1]);
        return -1;
    }

    // 6) Now we must read the remainder of the reply (BND.ADDR + BND.PORT).
    // The length depends on ATYP:
    int atyp = buf[3];
    int to_read = 0;
    switch (atyp) {
        case 0x01: // IPv4 => 4 bytes + 2 port
            to_read = 4 + 2;
            break;
        case 0x04: // IPv6 => 16 bytes + 2 port
            to_read = 16 + 2;
            break;
        case 0x03: // domain => 1 byte length + that many bytes + 2 port
            if (full_recv(proxy_fd, buf, 1) < 0) {
                perror("recv(domain-len)");
                return -1;
            }
            to_read = buf[0] + 2;  // domain length + 2 for port
            break;
        default:
            fprintf(stderr, "SOCKS5: unknown ATYP=0x%02x in reply\n", atyp);
            return -1;
    }
    if (to_read > 0) {
        if (full_recv(proxy_fd, buf, to_read) < 0) {
            perror("recv(connect-reply-body)");
            return -1;
        }
    }

    // If we got here, the CONNECT succeeded.
    fprintf(stderr, "[toralizer] SOCKS5 CONNECT success.\n");
    return 0;
}

// The hooked connect() function.
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    // Resolve the real connect() once
    if (!real_connect) {
        real_connect = dlsym(RTLD_NEXT, "connect");
        if (!real_connect) {
            fprintf(stderr, "Error resolving real connect(): %s\n", dlerror());
            exit(1);
        }
    }

    // Only handle IPv4 or IPv6
    if (addr->sa_family != AF_INET && addr->sa_family != AF_INET6) {
        return real_connect(sockfd, addr, addrlen);
    }

    // Cast to check IP/port
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *target4 = (const struct sockaddr_in *)addr;
        // If the target is the Tor proxy itself, bypass hooking to avoid recursion.
        if (target4->sin_addr.s_addr == inet_addr(TOR_PROXY_IP) &&
            target4->sin_port == htons(TOR_PROXY_PORT)) {
            return real_connect(sockfd, addr, addrlen);
        }
    } else if (addr->sa_family == AF_INET6) {
        // For IPv6, do a similar check if the address is [::1]:9050 (if your Tor is bound there).
        // Or skip if you only run Tor on 127.0.0.1 (IPv4).
        // (Example: in6addr_loopback is ::1 in network byte order.)
        const struct sockaddr_in6 *target6 = (const struct sockaddr_in6 *)addr;
        static const struct in6_addr loopback = IN6ADDR_LOOPBACK_INIT;
        if (memcmp(&target6->sin6_addr, &loopback, sizeof(loopback)) == 0 &&
            target6->sin6_port == htons(TOR_PROXY_PORT)) {
            return real_connect(sockfd, addr, addrlen);
        }
    }

    // 1) Create our socket to connect to the Tor proxy
    int proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_fd < 0) {
        perror("socket(tor-proxy)");
        return -1;
    }

    struct sockaddr_in proxy_addr;
    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port   = htons(TOR_PROXY_PORT);
    proxy_addr.sin_addr.s_addr = inet_addr(TOR_PROXY_IP);

    // 2) Connect to Tor's SOCKS5 proxy
    if (real_connect(proxy_fd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("connect(tor-proxy)");
        close(proxy_fd);
        return -1;
    }
    fprintf(stderr, "[toralizer] Connected to Tor proxy at %s:%d.\n",
            TOR_PROXY_IP, TOR_PROXY_PORT);

    // 3) Perform the SOCKS5 handshake to connect to the real destination
    if (socks5_handshake(proxy_fd, addr) < 0) {
        fprintf(stderr, "[toralizer] SOCKS5 handshake failed.\n");
        close(proxy_fd);
        return -1;
    }

    // 4) If handshake succeeded, map 'sockfd' to 'proxy_fd' using dup2()
    if (dup2(proxy_fd, sockfd) < 0) {
        perror("dup2");
        close(proxy_fd);
        return -1;
    }
    close(proxy_fd);

    // The calling application now thinks 'sockfd' is connected to the real server,
    // but it’s actually going through Tor’s SOCKS5 proxy.
    return 0;
}

