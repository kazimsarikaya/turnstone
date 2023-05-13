/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___OS_IO_H
#define ___OS_IO_H 0

#include <types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define PROT_READ    0x1
#define PROT_WRITE   0x2
#define MAP_SHARED   0x01
#define MAP_PRIVATE  0x02
#define MAP_FIXED    0x10
#define MAP_ANONYMOUS 0x20

typedef long FILE;
FILE*   fopen(const char* filename, const char* mode);
size_t  fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t  fread ( void* ptr, size_t size, size_t count, FILE* stream);
size_t  ftell(FILE* stream);
int32_t fseek (FILE* stream, size_t offset, int32_t origin);
size_t  fclose(FILE* stream);
int32_t fflush(FILE* stream);
int32_t fileno(FILE * stream);
int32_t unlink(const char_t * pathname);
FILE*   tmpfile(void);
void*   mmap(void * addr, size_t length, int32_t prot, int32_t flags, int32_t fd, int32_t offset);
int     munmap(void * addr, size_t length);
void    exit(int status);

#endif
