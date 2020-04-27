/**
 * @file strings.h>
 * @brief main string operations interface
 */
#ifndef ___STRINGS_H
/*! prevent duplicate header error macro */
#define ___STRINGS_H 0

#include <types.h>

/**
 * @brief calculates null ended string's length
 * @param[in]  string string to calculate length
 * @return length
 */
size_t strlen(char_t* string);

/**
 * @brief compares two string
 * @param  string1 first string
 * @param  string2 second string
 * @return <0 if string1>string2, 0 string1=string2, >0 string1<string2
 */
uint8_t strcmp(char_t* string1, char_t* string2);

/**
 * @brief copies source string to destination string
 * @param  source      source string
 * @param  destination destination string
 * @return 0 on success
 *
 * NULL will not be copied. destination should be equal or greater then source.
 * destination should have space for NULL.
 */
uint8_t strcpy(char_t* source, char_t* destination);

/**
 * @brief reverse a string
 * @param[in]  source string to be reversed
 * @return  reversed string
 *
 * allocates new space, hence return value needs be freed, else leak will be happened
 */
char* strrev(char_t* source);

/**
 * @brief converts string into number
 * @param[in]  source string represents number in base
 * @param[in]  base   base of number inside string
 * @return number
 */
number_t ato_base(char_t* source, number_t base);

/*! ato_base macro for base 10 */
#define atoi(number) ato_base(number, 10)
/*! ato_base macro for base 16 */
#define atoh(number) ato_base(number, 16)

/**
 * @brief convers number to its string representation
 * @param[in]  number number to be converted string
 * @param[in]  base   base value of conversion
 * @return  string represents number
 *
 * return value should be freed or memory leak will be happened
 */
char_t* ito_base(number_t number, number_t base);

/*! ito_base macro for base 10 */
#define itoa(number) ito_base(number, 10)

/**
 * @brief returns hex string of number
 * @param[in]  number the nubmer to be converted into hex string
 * @return hex string value
 *
 * return value should be free or memory leak will be happened.
 * length of hex string will be 2 time then max size of number filled with zeros.
 */
char_t* itoh(size_t number);

#endif
