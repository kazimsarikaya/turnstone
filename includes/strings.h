/**
 * @file strings.h>
 * @brief main string operations interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___STRINGS_H
/*! prevent duplicate header error macro */
#define ___STRINGS_H 0

#include <types.h>
#include <memory.h>
#include <sunday_match.h>

/**
 * @brief calculates null ended string's length
 * @param[in]  string string to calculate length
 * @return length
 */
size_t strlen(const char_t* string);

/**
 * @brief compares two string
 * @param  string1 first string
 * @param  string2 second string
 * @return <0 if string1>string2, 0 string1=string2, >0 string1<string2
 */
int8_t strcmp(const char_t* string1, const char_t* string2);

/**
 * @brief compares two string with first n bytes
 * @param  string1 first string
 * @param  string2 second string
 * @param  n first n bytes
 * @return <0 if string1>string2, 0 string1=string2, >0 string1<string2
 */
int8_t strncmp(const char_t* string1, const char_t* string2, size_t n);

/**
 * @brief checks str starts with prefix
 * @param  str string for look
 * @param  prefix to compare
 * @return 0 if str starts with prefix, else -1
 */
int8_t strstarts(const char_t* str, const char_t* prefix);

/**
 * @brief checks str ends with suffix
 * @param  str string for look
 * @param  suffix to compare
 * @return 0 if str ends with suffix, else -1
 */
int8_t strends(const char_t* str, const char_t* suffix);

/**
 * @brief checks str contains pattern
 * @param  str string for look
 * @param  pattern to check
 * @return -1 if str does not contain pattern, else position
 */
#define strcontains(str, pattern) sunday_match((uint8_t*)str, strlen(str), (uint8_t*)pattern, strlen(pattern))

/**
 * @brief concanates two string and returns new one
 * @param  string1 first string
 * @param  string2 second string
 * @return new string, needs freeing or memory leak
 */
char_t* strcat_at_heap(memory_heap_t* heap, const char_t* string1, const char_t* string2);

/*! strcat at default heap */
#define strcat(s1, s2) strcat_at_heap(NULL, s1, s2)

/**
 * @brief copies source string to destination string
 * @param  source      source string
 * @param  destination destination string
 * @return 0 on success
 *
 * NULL will not be copied. destination should be equal or greater then source.
 * destination should have space for NULL.
 */
int8_t strcpy(const char_t* source, char_t* destination);

/**
 * @brief reverse a string
 * @param[in]  source string to be reversed
 * @return  reversed string
 *
 * allocates new space, hence return value needs be freed, else leak will be happened
 */
char* strrev(const char_t* source);

/**
 * @brief converts string into number
 * @param[in]  source string represents number in base
 * @param[in]  base   base of number inside string
 * @return number
 */
number_t ato_base(const char_t* source, number_t base);

/*! ato_base macro for base 10 */
#define atoi(number) ato_base(number, 10)

/**
 * @brief converts string into number
 * @param[in]  source string represents number in base
 * @param[in]  base   base of number inside string
 * @return number
 */
unumber_t atou_base(const char_t* source, number_t base);


/*! atou_base macro for base 10 */
#define atou(number) atou_base(number, 16)
/*! atou_base macro for base 16 */
#define atoh(number) atou_base(number, 16)

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
/*! ito_base macro for base 16 */
#define itoh(number) ito_base(number, 16)


/**
 * @brief convers unsigned number to its string representation
 * @param[in]  number unsigned number to be converted string
 * @param[in]  base   base value of conversion
 * @return  string represents number
 *
 * return value should be freed or memory leak will be happened
 */
char_t* uto_base(unumber_t number, number_t base);

#define utoa(number) uto_base(number, 10)
#define utoh(number) uto_base(number, 16)

/**
 * @brief duplicate string at heap
 * @param[in] heap destination heap
 * @param[in] src source string
 * @param[in] length of destination string
 * @return duplicate at heap
 */
char_t* strndup_at_heap(memory_heap_t* heap, const char_t* src, size_t n);

/*! string duplicate at heap */
#define strdup_at_heap(heap, src) strndup_at_heap(heap, src, strlen(src))

/*! string duplicate at default heap */
#define strdup(src) strndup_at_heap(NULL, src, strlen(src))

/*! string duplicate with length at default heap */
#define strndup(src, n) strndup_at_heap(NULL, src, n)

/**
 * @brief splits string into array
 * @param[in] str string to tokinize
 * @param[in] token split char
 * @param[out] lengths array of each splitted part's length
 * @param[out] count splitted array length
 * @return array of starts of each part
 */
char_t** strsplit(const char_t* str, const char_t token, int64_t** lengths, int64_t* count);

/**
 * @brief converts string to upper case
 * @param[in] str string to upper case
 * @return input string
 */
char_t* strupper(char_t* str);

/**
 * @brief duplicates string and converts it to upper case
 * @param[in] str string to upper case
 * @return new uppercased string
 */
char_t* struppercopy(const char_t* str);

/**
 * @brief converts string to lower case
 * @param[in] str string to lower case
 * @return input string
 */
char_t* strlower(char_t* str);

/**
 * @brief duplicates string and converts it to lower case
 * @param[in] str string to lower case
 * @return new lowercased string
 */
char_t* strlowercopy(const char_t* str);

char_t* strtrim_right(char_t* str);

int8_t str_is_upper(char_t* str);

int64_t  wchar_size(const wchar_t* str);
char_t*  wchar_to_char(wchar_t* src);
wchar_t* char_to_wchar(const char_t* str);

int64_t  lchar_size(const lchar_t* str);
char_t*  lchar_to_char(lchar_t* src);
lchar_t* char_to_lchar(char_t* str);

uint64_t strhash(const char_t* input);

char_t* sprintf(const char_t* format, ...);
char_t* vsprintf(const char_t* format, va_list args);


#endif
