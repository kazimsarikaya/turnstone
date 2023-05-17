#include <types.h>
#include <memory.h>
#include <strings.h>
#include <utils.h>



void __attribute__((constructor)) __init_srand_seed(void);
void                              __clean_tmpfiles(void);


typedef struct timeval {
    uint64_t tv_sec;
    uint64_t tv_usec;
} timeval_t;

typedef struct timezone {
    int32_t tz_minuteswest;
    int32_t tz_dsttime;
} timezone_t;

typedef struct timespec {
    uint64_t tv_sec;
    uint64_t tv_nsec;
} timespec_t;

typedef struct tms {
    uint64_t tms_utime;
    uint64_t tms_stime;
    uint64_t tms_cutime;
    uint64_t tms_cstime;
} tms_t;

typedef uint32_t time_t;

int32_t  gettimeofday(timeval_t* tv, timezone_t* tz);
uint32_t time(time_t* t);

int32_t gettimeofday(timeval_t* tv, timezone_t* tz) {
    if(tv) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }

    if(tz) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }

    int64_t ret = 0;

    asm volatile (
        "mov $0x60, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (tv), "S" (tz)
        );

    return ret;
}

uint32_t time(time_t* t) {

    timeval_t tv = {0};

    gettimeofday(&tv, NULL);

    if(t) {
        *t = tv.tv_sec;
    }

    return tv.tv_sec;
}

int64_t errno = 0;
uint32_t __srand_seed = 0;

void    srand(uint32_t seed);
int32_t rand(void);

void srand(uint32_t seed) {
    __srand_seed = seed;
}

int32_t rand(void) {
    __srand_seed = __srand_seed * 1103515245 + 12345;

    return (__srand_seed / 65536) % 32768;
}

__attribute__((naked, force_align_arg_pointer)) int32_t _tos_start(void);

int32_t main(int32_t argc, char_t** argv, char_t** envp);
void    __premain(void);
void    __postmain(void);

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

#define S_IRWXU 0000700
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRWXG 0000070
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IRWXO 0000007
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef uint64_t FILE;

int32_t  open(const char_t* pathname, int32_t flags, int32_t mode);
int64_t  read(uint64_t fd, void* buf, uint64_t count);
int64_t  write(uint64_t fd, const void* buf, uint64_t count);
int64_t  close(uint64_t fd);
uint64_t lseek(uint64_t fd, uint64_t offset, int32_t whence);
void*    mmap(uint64_t addr, uint64_t length, int32_t prot, int32_t flags, uint64_t fd, uint64_t offset);
int32_t  mprotect(void* addr, uint64_t len, int32_t prot);
int64_t  munmap(void* addr, uint64_t length);
int64_t  exit(int64_t status);
int32_t  unlink(const char_t * pathname);


//int32_t printf(const char_t* format, ...);
int32_t vprintf(const char_t* format, va_list arg );
int64_t video_print(const char_t* buf);

int32_t __popcountdi2(uint64_t x);

int32_t printf(const char_t* format, ...) {
    va_list arg;
    int done;

    va_start(arg, format);
    done = vprintf(format, arg);
    va_end(arg);

    return done;
}

int64_t video_print(const char_t* buf) {
    return write(1, buf, strlen(buf));
}

int32_t vprintf(const char_t* fmt, va_list args) {
    size_t cnt = 0;

    while (*fmt) {
        char_t data = *fmt;

        if(data == '%') {
            fmt++;
            int8_t wfmtb = 1;
            char_t buf[257] = {0};
            char_t ito_buf[64];
            int32_t val = 0;
            char_t* str = NULL;
            int32_t slen = 0;
            number_t ival = 0;
            unumber_t uval = 0;
            int32_t idx = 0;
            int8_t l_flag = 0;
            int8_t sign = 0;
            char_t fto_buf[128];
            // float128_t fval = 0; // TODO: float128_t ops
            float64_t fval = 0;
            number_t prec = 6;

            while(1) {
                wfmtb = 1;

                switch (*fmt) {
                case '0':
                    fmt++;
                    val = *fmt - 0x30;
                    fmt++;
                    if(*fmt >= '0' && *fmt <= '9') {
                        val = val * 10 + *fmt - 0x30;
                        fmt++;
                    }
                    wfmtb = 0;
                    break;
                case '.':
                    fmt++;
                    prec = *fmt - 0x30;
                    fmt++;
                    wfmtb = 0;
                    break;
                case 'c':
                    val = va_arg(args, int32_t);
                    buf[0] = (char_t)val;
                    video_print(buf);
                    cnt++;
                    fmt++;
                    break;
                case 's':
                    str = va_arg(args, char_t*);
                    video_print(str);
                    cnt += strlen(str);
                    fmt++;
                    break;
                case 'i':
                case 'd':
                    if(l_flag == 2) {
                        ival = va_arg(args, int64_t);
                    } else if(l_flag == 1) {
                        ival = va_arg(args, int64_t);
                    }
                    if(l_flag == 0) {
                        ival = va_arg(args, int32_t);
                    }

                    itoa_with_buffer(ito_buf, ival);
                    slen = strlen(ito_buf);

                    if(ival < 0) {
                        sign = 1;
                        slen -= 2;
                    }

                    for(idx = 0; idx < val - slen; idx++) {
                        buf[idx] = '0';
                        cnt++;
                    }

                    if(ival < 0) {
                        buf[0] = '-';
                    }

                    video_print(buf);
                    memory_memclean(buf, 257);
                    video_print(ito_buf + sign);
                    memory_memclean(ito_buf, 64);

                    cnt += slen;
                    fmt++;
                    l_flag = 0;
                    break;
                case 'u':
                    if(l_flag == 2) {
                        uval = va_arg(args, uint64_t);
                    } else if(l_flag == 1) {
                        uval = va_arg(args, uint64_t);
                    }
                    if(l_flag == 0) {
                        uval = va_arg(args, uint32_t);
                    }

                    utoa_with_buffer(ito_buf, uval);
                    slen = strlen(ito_buf);

                    for(idx = 0; idx < val - slen; idx++) {
                        buf[idx] = '0';
                        cnt++;
                    }

                    video_print(buf);
                    memory_memclean(buf, 257);
                    video_print(ito_buf);
                    memory_memclean(ito_buf, 64);

                    cnt += slen;
                    fmt++;
                    break;
                case 'l':
                    fmt++;
                    wfmtb = 0;
                    l_flag++;
                    break;
                case 'p':
                    l_flag = 1;
                    video_print("0x");
                    nobreak;
                case 'x':
                case 'h':
                    if(l_flag == 2) {
                        uval = va_arg(args, uint64_t);
                    } else if(l_flag == 1) {
                        uval = va_arg(args, uint64_t);
                    }
                    if(l_flag == 0) {
                        uval = va_arg(args, uint32_t);
                    }

                    utoh_with_buffer(ito_buf, uval);
                    slen = strlen(ito_buf);

                    for(idx = 0; idx < val - slen; idx++) {
                        buf[idx] = '0';
                        cnt++;
                    }

                    video_print(buf);
                    memory_memclean(buf, 257);
                    video_print(ito_buf);
                    memory_memclean(ito_buf, 64);
                    cnt += slen;
                    fmt++;
                    l_flag = 0;
                    break;
                case '%':
                    buf[0] = '%';
                    video_print(buf);
                    memory_memclean(buf, 257);
                    fmt++;
                    cnt++;
                    break;
                case 'f':
                    if(l_flag == 2) {
                        // fval = va_arg(args, float128_t); // TODO: float128_t ops
                    } else  {
                        fval = va_arg(args, float64_t);
                    }

                    ftoa_with_buffer_and_prec(fto_buf, fval, prec); // TODO: floating point prec format
                    slen = strlen(fto_buf);
                    video_print(fto_buf);
                    memory_memclean(fto_buf, 128);

                    cnt += slen;
                    fmt++;
                    break;
                default:
                    break;
                }

                if(wfmtb) {
                    break;
                }
            }

        } else {
            size_t idx = 0;
            char_t buf[17];
            memory_memclean(buf, 17);

            while(*fmt) {
                if(idx == 16) {
                    video_print(buf);
                    idx = 0;
                    memory_memclean(buf, 17);
                }
                if(*fmt == '%') {
                    video_print(buf);
                    memory_memclean(buf, 17);

                    break;
                }
                buf[idx++] = *fmt;
                fmt++;
                cnt++;
            }
            video_print(buf);
        }
    }


    return cnt;
}

int64_t read(uint64_t fd, void* buf, uint64_t count) {
    int64_t ret = 0;

    asm volatile (
        "mov $0, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (fd), "S" (buf), "d" (count)
        );

    if(ret < 0) {
        errno = ret;
        return -1;
    }

    return ret;
}

int64_t write(uint64_t fd, const void* buf, uint64_t count) {
    int64_t ret = 0;

    asm volatile (
        "mov $1, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (fd), "S" (buf), "d" (count)
        );

    if(ret < 0) {
        errno = ret;
        return -1;
    }

    return ret;
}

int32_t open(const char_t* pathname, int32_t flags, int32_t mode) {
    int64_t ret = 0;

    asm volatile (
        "mov $2, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (pathname), "S" (flags), "d" (mode)
        );

    return ret;
}

int64_t close(uint64_t fd) {
    int64_t ret = 0;

    asm volatile (
        "mov $3, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (fd)
        );

    return ret;
}

uint64_t lseek(uint64_t fd, uint64_t offset, int32_t whence) {
    int64_t ret = 0;

    asm volatile (
        "mov $8, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (fd), "S" (offset), "d" (whence)
        );

    return ret;
}

void* mmap(uint64_t addr, uint64_t length, int32_t prot, int32_t flags, uint64_t fd, uint64_t offset) {
    void* ret = 0;
    register long r10 asm ("r10") = flags;
    register long r8 asm ("r8") = fd;
    register long r9 asm ("r9") = offset;

    asm volatile (
        "mov $9, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (addr), "S" (length), "d" (prot), "r" (r10), "r" (r8), "r" (r9)
        );

    int64_t err = (int64_t)ret;

    if(err < 0) {
        errno = err;
        ret = (void*)-1;
    }

    return ret;
}

int32_t mprotect(void* addr, uint64_t length, int32_t prot) {
    int64_t ret = 0;

    asm volatile (
        "mov $10, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (addr), "S" (length), "d" (prot)
        );

    if(ret != 0) {
        errno = ret;
        ret = -1;
    }

    return ret;
}

int64_t munmap(void* addr, uint64_t length) {
    int64_t ret = 0;

    asm volatile (
        "mov $11, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (addr), "S" (length)
        );

    if(ret != 0) {
        errno = ret;
        ret = -22;
    }

    return ret;
}

int64_t exit(int64_t status) {
    __clean_tmpfiles();

    int64_t ret = 0;

    asm volatile (
        "mov $60, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (status)
        );

    return ret;
}

int32_t unlink(const char_t* pathname) {
    int64_t ret = 0;

    asm volatile (
        "mov $87, %%rax\n"
        "syscall\n"
        : "=a" (ret)
        : "D" (pathname)
        );

    return ret;
}

int32_t __popcountdi2(uint64_t x) {
    int32_t ret = 0;

    asm volatile (
        "popcnt %%rax, %%rax\n"
        : "=a" (ret)
        : "a" (x)
        );

    return ret;
}
FILE*    fopen(const char_t* filename, const char_t* mode);
int32_t  fclose(const FILE* stream);
int64_t  fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
int64_t  fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
uint64_t ftell(const FILE* stream);
int32_t  fileno(const FILE * stream);
FILE*    tmpfile(void);
uint64_t fseek (const FILE* stream, size_t offset, int32_t origin);
int32_t  fflush(FILE* stream);

int32_t fflush(FILE* stream) {
    if(!stream) {
        return -1;
    }

    return 0;
}

FILE* fopen(const char_t* filename, const char_t* mode) {

    if(!filename || !mode) {
        return 0;
    }

    int32_t i_mode = 0;

    boolean_t append = false;
    boolean_t write = false;

    while(*mode) {
        switch(*mode) {
        case 'r':
            i_mode |= O_RDONLY;
            break;
        case 'w':
            i_mode |= O_WRONLY;
            write = true;
            break;
        case 'a':
            i_mode |= O_APPEND;
            append = true;
            break;
        case '+':
            i_mode |= O_RDWR;
            write = true;
            break;
        case 'b':
            break;
        default:
            break;
        }
        mode++;
    }

    if(write && !append) {
        i_mode |= O_TRUNC | O_CREAT;
    }

    int32_t fd = open(filename, i_mode, S_IRUSR | S_IWUSR);

    if(fd <= 0) {
        errno = fd;

        return 0;
    }

    uint64_t ret = fd;

    return (FILE*)ret;
}

int32_t fclose(const FILE* stream) {
    return close(fileno(stream));
}

int64_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    int64_t ret = 0;

    int64_t total_read = 0;
    boolean_t return_total = false;

    if(size == 1) {
        size = nmemb;
        nmemb = 1;
        return_total = true;
    }

    uint8_t* data = ptr;

    for(size_t i = 0; i < nmemb; i++) {
        int64_t t_ret = read(fileno(stream), data, size);

        if(t_ret < 0) {
            return t_ret;
        }

        total_read += t_ret;

        ret++;
        data += size;
    }

    if(return_total) {
        return total_read;
    }

    return ret;
}

int64_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    int64_t ret = 0;

    int64_t total_written = 0;
    boolean_t return_total = false;

    if(size == 1) {
        size = nmemb;
        nmemb = 1;
        return_total = true;
    }

    const uint8_t* data = ptr;

    for(size_t i = 0; i < nmemb; i++) {
        int64_t t_ret = write(fileno(stream), data, size);

        if(t_ret < 0) {
            return t_ret;
        }

        total_written += t_ret;
        ret++;
        data += size;
    }

    if(return_total) {
        return total_written;
    }

    return ret;
}

uint64_t ftell(const FILE* stream) {
    return lseek((uint64_t)stream, 0, SEEK_CUR);
}

int32_t fileno(const FILE* stream) {
    uint64_t ret = (uint64_t)stream;
    return ret;
}

#define TMPFILE_MAX 32
char_t __tmpfiles[TMPFILE_MAX][32] = {0};
int32_t __tmpfile_index = 0;

FILE* tmpfile(void) {
    memory_memcopy("/tmp/tf.", __tmpfiles[__tmpfile_index], strlen("/tmp/tf."));

    char_t* index = __tmpfiles[__tmpfile_index] + strlen("/tmp/tf.");

    for(int32_t i = 0; i < 16; i++) {
        index[i] = 'a' + (rand() % 26);
    }

    int32_t fd = open(__tmpfiles[__tmpfile_index], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if(fd <= 0) {
        errno = fd;

        return 0;
    }

    __tmpfile_index++;

    uint64_t ret = fd;

    return (FILE*)ret;
}

uint64_t fseek(const FILE* stream, size_t offset, int32_t origin) {
    return lseek((uint64_t)stream, offset, origin);
}

int32_t _tos_start(void) {
    asm volatile (
        "mov %rsp, %rbp\n"
        "call __premain\n"
        "jnz __exit_tos_start\n"
        "mov 0(%rbp), %rdi\n"
        "lea 8(%rbp), %rsi\n"
        "mov %rdi, %rax\n"
        "add $2, %rax\n"
        "shl $3, %rax\n"
        "add %rbp, %rax\n"
        "lea (%rax), %rdx\n"
        "call main\n"
        "push %rax\n"
        "call __postmain\n"
        "pop %rax\n"
        "__exit_tos_start:mov %rax, %rdi\n"
        "call exit\n"
        );
}

extern void (*__init_array_start []) (void) __attribute__((weak));
extern void (*__init_array_end []) (void) __attribute__((weak));
extern void (*__fini_array_start []) (void) __attribute__((weak));
extern void (*__fini_array_end []) (void) __attribute__((weak));

void __premain(void) {
    uint64_t if_start = (uint64_t)__init_array_start;
    uint64_t if_end = (uint64_t)__init_array_end;

    uint64_t if_count = (if_end - if_start) / sizeof(void*);

    for(uint64_t i = 0; i < if_count; i++) {
        __init_array_start[i]();
    }

}

void __postmain(void) {
    uint64_t ff_start = (uint64_t)__fini_array_start;
    uint64_t ff_end = (uint64_t)__fini_array_end;

    uint64_t ff_count = (ff_end - ff_start) / sizeof(void*);

    for(uint64_t i = 0; i < ff_count; i++) {
        __fini_array_start[i]();
    }
}

void __init_srand_seed(void) {
    printf("init_srand_seed\n");
    timeval_t tv = {0};
    gettimeofday(&tv, 0);
    __srand_seed = tv.tv_usec;
    printf("srand_seed: %d\n", __srand_seed);
}

void __clean_tmpfiles(void) {
    for(int32_t i = 0; i < TMPFILE_MAX; i++) {
        if(__tmpfiles[i][0] != 0) {
            unlink(__tmpfiles[i]);
        }
    }

    printf("clean_tmpfiles\n");
}

