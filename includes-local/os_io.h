/**
 * @file os_io.h
 * @brief Host OS IO header.
 * @details This file contains declarations of functions that are used to on host OS.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___OS_IO_H
#define ___OS_IO_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define PROT_READ    0x1
#define PROT_WRITE   0x2
#define PROT_EXEC    0x4
#define MAP_SHARED   0x01
#define MAP_PRIVATE  0x02
#define MAP_FIXED    0x10
#define MAP_HUGE_2MB (21 << 26)
#define MAP_SHARED_VALIDATE 0x03
#define MAP_ANONYMOUS 0x20
#define MAP_SYNC       0x80000
#define MS_SYNC        4
#define MAP_FILE       0
#define MAP_FAILED     ((void *) -1)

typedef long FILE;
FILE*          fopen(const char* filename, const char* mode);
size_t         fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t         fread (void* ptr, size_t size, size_t count, FILE* stream);
size_t         ftell(FILE* stream);
int32_t        fseek (FILE* stream, size_t offset, int32_t origin);
size_t         fclose(FILE* stream);
int32_t        fflush(FILE* stream);
int32_t        fprintf(FILE* stream, const char* format, ...);
int32_t        fscanf(FILE* stream, const char* format, ...);
int32_t        putc(int32_t c, FILE* stream);
int32_t        getc(FILE* stream);
int32_t        fileno(FILE * stream);
int32_t        unlink(const char_t * pathname);
FILE*          tmpfile(void);
void*          mmap(void * addr, size_t length, int32_t prot, int32_t flags, int32_t fd, int32_t offset);
int            munmap(void * addr, size_t length);
int            msync(void * addr, size_t length, int32_t flags);
_Noreturn void exit(int32_t status);


#define SIGABRT 6
#define SIGPIPE 13

typedef void (*sighandler_t)(int32_t);

#define SIG_IGN ((sighandler_t)1)

sighandler_t signal(int32_t signum, sighandler_t handler);

typedef struct in_addr {
    uint32_t s_addr; // load with inet_pton()
} in_addr_t;

typedef uint16_t in_port_t;

typedef uint16_t sa_family_t;

typedef uint32_t socklen_t;

enum {
    AF_INET = 2,
    SOCK_STREAM = 1,
    SOL_SOCKET = 1,
    SO_REUSEADDR = 2,
    SO_REUSEPORT = 15,
};

enum {
    INADDR_ANY = 0x00000000,
};

#define INET_ADDRSTRLEN 16

struct sockaddr_in {
    sa_family_t    sin_family; // address family: AF_INET
    in_port_t      sin_port; // port in network byte order
    struct in_addr sin_addr; // internet address
    unsigned char  sin_zero[8]; // padding
};

struct sockaddr {
    sa_family_t sa_family; // address family
    char_t      sa_data[14]; // up to 14 bytes of direct address
};

int32_t     socket(int domain, int type, int protocol);
int32_t     close(int32_t fd);
int32_t     bind(int32_t sockfd, const struct sockaddr * addr, socklen_t addrlen);
int32_t     listen(int32_t sockfd, int32_t backlog);
int32_t     accept(int32_t sockfd, struct sockaddr * addr, socklen_t * addrlen);
int32_t     setsockopt(int32_t sockfd, int32_t level, int32_t optname, const void * optval, socklen_t optlen);
int32_t     send(int32_t sockfd, const void * buf, size_t len, int32_t flags);
int32_t     recv(int32_t sockfd, void * buf, size_t len, int32_t flags);
const char* inet_ntop(int32_t af, const void * src, char_t * dst, socklen_t size);
int32_t     htons(uint16_t hostshort);
int32_t     ntohs(uint16_t netshort);

#ifdef __cplusplus
}
#endif

#endif
