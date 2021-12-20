#ifndef ___OS_IO_H
#define ___OS_IO_H 0

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef long FILE;
FILE* fopen(const char* filename, const char* mode);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fread (void* ptr, size_t size, size_t count, FILE* stream);
size_t ftell(FILE* stream);
int32_t fseek (FILE* stream, size_t offset, int32_t origin);
size_t fclose(FILE* stream);
int fflush(FILE* stream);
int fprintf(FILE* stream, const char* format, ...);
int fscanf(FILE* stream, const char* format, ...);

#endif
