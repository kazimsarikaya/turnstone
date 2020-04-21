#ifndef ___TYPES_H
#define ___TYPES_H 0

#if ___BITS == 16
asm (".code16gcc");
#endif

#define char_t char
#define int8_t char
#define uchar_t unsigned char
#define uint8_t unsigned char
#define int16_t short
#define uint16_t unsigned short
#define int32_t int
#define uint32_t unsigned int

#if ___BITS == 16

typedef struct {
	int32_t part_low;
	int32_t part_high;
} __attribute__ ((packed)) int64_t;

typedef struct {
	uint32_t part_low;
	uint32_t part_high;
} __attribute__ ((packed)) uint64_t;

#define EXT_INT64_GET_BYTE(i, bo) (bo < 4 ? ((i.part_low >> (bo * 4)) & 0XFF) : ((i.part_high >> ((bo - 4) * 4)) & 0XFF))

#define reg_t uint16_t
#define regext_t uint32_t
#define size_t uint32_t
#define number_t int32_t

#elif ___BITS == 64

#define int64_t long
#define uint64_t unsigned long

#define EXT_INT64_GET_BYTE(i, bo) (i >> (bo * 4) & 0xFF)

#define reg_t uint64_t
#define regext_t uint64_t
#define size_t uint64_t
#define number_t int64_t

#endif


#endif
