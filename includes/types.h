/**
 * @file types.h
 * @brief cpu data types
 */
#ifndef ___TYPES_H
/*! prevent duplicate header error macro */
#define ___TYPES_H 0

/*! unused parameter warning supress macro */
#define UNUSED(x) (void)(x)

#define NULL 0

/*! char type */
#define char_t char
/*! signed byte type */
#define int8_t char
/*! unsigned char type */
#define uchar_t unsigned char
/*! unsigned byte type */
#define uint8_t unsigned char
/*! boolean type */
typedef uint8_t boolean_t;
/*! signed word (two bytes) type */
#define int16_t short
/*! wide char (two bytes) type */
#define wchar_t unsigned short
/*! unsigned word (two bytes) type */
#define uint16_t unsigned short
/*! signed double word (four bytes) type */
#define int32_t int
/*! unsigned double word (four bytes) type */
#define uint32_t unsigned int

/*! signed quad word (eight bytes) type */
#define int64_t long long
/*! unsigned quad word (eight bytes) type */
#define uint64_t unsigned long long

/**
 * @brief gets byte at offset from quad word
 * @param[in] i quad word
 * @param[in] bo byte offset
 * @return byte at offset
 */
#define EXT_INT64_GET_BYTE(i, bo) (i >> (bo * 4) & 0xFF)

/*! cpu registery type at long mode */
#define reg_t uint64_t
/*! cpu extended registery type at long mode*/
#define regext_t uint64_t
/*! size of objects type at long mode */
#define size_t uint64_t
/*! alias for signed quad word at long mode */
#define number_t int64_t
/*! alias for signed quad word at long mode */
#define unumber_t uint64_t
/*! alias for 32-bit precision floating point */
#define float32_t float
/*! alias for 64-bit precision floating point */
#define float64_t double
/*! alias for 64-bit precision floating point */
#define float128_t long double

/* 128 bit types */
/*! alias for 128-bit signed integer */
#define int128_t __int128
/*! alias for 128-bit signed integer */
#define uint128_t unsigned __int128

#ifdef ___TESTMODE
int printf(const char* format, ...);
#endif

#define va_list     __builtin_va_list
#define va_start(v, f) __builtin_va_start(v, f);
#define va_end(v)       __builtin_va_end(v);
#define va_arg(v, a)   __builtin_va_arg(v, a);

#endif
