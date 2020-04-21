#ifndef ___STRINGS_H
#define ___STRINGS_H 0

#include <types.h>

size_t strlen(char_t*);
uint8_t strcmp(char_t*, char_t*);
uint8_t strcpy(char_t*, char_t*);
char* strrev(char_t*);
number_t ato_base(char_t*, number_t);
#define atoi(number) ato_base(number, 10)
#define atoh(number) ato_base(number, 16)
char_t* ito_base(number_t, number_t);
#define itoa(number) ito_base(number, 10)
#if ___BITS == 16
char_t* itoh(uint32_t);
#elif ___BITS == 64
char_t* itoh(uint64_t);
#endif
size_t strindexof(char_t*, char_t*);

#endif
