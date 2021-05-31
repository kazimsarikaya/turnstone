/**
 * @file types.h
 * @brief cpu data types
 */
#ifndef ___TYPES_H
/*! prevent duplicate header error macro */
#define ___TYPES_H 0

#if ___BITS == 16
asm (".code16gcc");
#endif

/*! unused parameter warning supress macro */
#define UNUSED(x) (void)(x)

/*! char type */
#define char_t char
/*! signed byte type */
#define int8_t char
/*! unsigned char type */
#define uchar_t unsigned char
/*! unsigned byte type */
#define uint8_t unsigned char
/*! signed word (two bytes) type */
#define int16_t short
/*! unsigned word (two bytes) type */
#define uint16_t unsigned short
/*! signed double word (four bytes) type */
#define int32_t int
/*! unsigned double word (four bytes) type */
#define uint32_t unsigned int

#if ___BITS == 16 || DOXYGEN

/**
 * @struct int64_t
 * @brief signed quad word (eight bytes) type
 *
 * real mode needs signed quad word as struct
 */
typedef struct {
	int32_t part_low; ///< low part signed double word of signed quad word
	int32_t part_high; ///< high part signed double word of signed quad word
} __attribute__ ((packed)) int64_t; ///< short hand for struct

/**
 * @struct uint64_t
 * @brief unsigned quad word (eight bytes) type
 *
 * real mode needs unsigned quad word as struct
 */
typedef struct {
	uint32_t part_low; ///< low part unsigned double word of unsigned quad word
	uint32_t part_high; ///< low part unsigned double word of unsigned quad word
} __attribute__ ((packed)) uint64_t; ///< short hand for struct

/**
 * @brief gets byte at offset from quad word
 * @param[in] i quad word
 * @param[in] bo byte offset
 * @return byte at offset
 */
#define EXT_INT64_GET_BYTE(i, bo) (bo < 4 ? ((i.part_low >> (bo * 4)) & 0XFF) : ((i.part_high >> ((bo - 4) * 4)) & 0XFF))

/*! cpu registery type at real mode*/
#define reg_t uint16_t
/*! cpu extended registery type at real mode */
#define regext_t uint32_t
/*! size of objects type at real mode */
#define size_t uint32_t
/*! alias for signed double word at real mode */
#define number_t int32_t

#elif ___BITS == 64 || DOXYGEN

/*! signed quad word (eight bytes) type */
#define int64_t long
/*! unsigned quad word (eight bytes) type */
#define uint64_t unsigned long

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
/*! alias for signed double word at long mode */
#define number_t int64_t

#endif

#ifdef ___TESTMODE
int printf(const char* format, ...);
#endif

#endif
