/**
 * @file types.h
 * @brief cpu data types
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___TYPES_H
/*! prevent duplicate header error macro */
#define ___TYPES_H 0

/*! unused parameter warning supress macro */
#define UNUSED(x) (void)(x)

#define NULL 0

/*! char type */
typedef char char_t;
/*! signed byte type */
typedef char int8_t;
/*! unsigned char type */
typedef unsigned char uchar_t;
/*! unsigned byte type */
typedef unsigned char uint8_t;
/*! boolean type */
#define true        1
#define false       0
typedef _Bool boolean_t;
/*! signed word (two bytes) type */
typedef short int16_t;
/*! wide char (two bytes) type */
typedef unsigned short wchar_t;
/*! unsigned word (two bytes) type */
typedef unsigned short uint16_t;
/*! signed double word (four bytes) type */
typedef int int32_t;
/*! unsigned double word (four bytes) type */
typedef unsigned int uint32_t;
/*! long char (four bytes) type */
typedef unsigned int lchar_t;

/*! signed quad word (eight bytes) type */
typedef long long int64_t;
/*! unsigned quad word (eight bytes) type */
typedef unsigned long long uint64_t;

/**
 * @brief gets byte at offset from quad word
 * @param[in] i quad word
 * @param[in] bo byte offset
 * @return byte at offset
 */
#define EXT_INT64_GET_BYTE(i, bo) (i >> (bo * 4) & 0xFF)

/*! cpu registery type at long mode */
typedef uint64_t reg_t;
/*! cpu extended registery type at long mode*/
typedef uint64_t regext_t;
/*! size of objects type at long mode */
typedef uint64_t size_t;
/*! alias for signed quad word at long mode */
#define number_t int64_t
/*! alias for signed quad word at long mode */
#define unumber_t uint64_t
/*! alias for 32-bit precision floating point */
typedef float float32_t;
/*! alias for 64-bit precision floating point */
typedef double float64_t;
/*! alias for 64-bit precision floating point */
typedef long double float128_t;

/* 128 bit types */
/*! alias for 128-bit signed integer */
typedef __int128 int128_t;
/*! alias for 128-bit signed integer */
typedef unsigned __int128 uint128_t;

typedef __builtin_va_list va_list;
#define va_start(v, f) __builtin_va_start(v, f);
#define va_end(v)       __builtin_va_end(v);
#define va_arg(v, a)   __builtin_va_arg(v, a);

#define MODULE(m) \
        __attribute__((section(".___module___"), visibility("internal"))) \
        const char_t ___module___[] = m;

#define nobreak __attribute__((fallthrough))
#endif
