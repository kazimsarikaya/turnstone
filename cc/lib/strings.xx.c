/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <strings.h>
#include <memory.h>
#include <utils.h>

MODULE("turnstone.lib");


size_t strlen(const char_t* string) {
    if(string == NULL) {
        return 0;
    }

    size_t ret = 0;

    while(string[ret]) {
        ret++;
    }

    return ret;
}

int8_t strcmp(const char_t* string1, const char_t* string2) {
    size_t len1 = strlen(string1);
    size_t len2 = strlen(string2);

    size_t minlen = MIN(len1, len2);

    int8_t res = memory_memcompare(string1, string2, minlen);

    if(res) {
        return res;
    }

    if(len1 == len2) {
        return 0;
    }

    return (minlen == len1) ? -1 : 1;
}

int8_t strcpy(const char_t* source, char_t* destination){
    if(source == NULL || destination == NULL) {
        return -1;
    }

    for(size_t i = 0; i < strlen(source); i++) {
        destination[i] = source[i];
    }

    return 0;
}

char_t* strrev(const char_t* source) {
    size_t len = strlen(source);
    if(len == 0) {
        return NULL;
    }
    char_t* dest = memory_malloc(sizeof(char_t) * len);
    if(dest == NULL) {
        return NULL;
    }
    for(size_t i = 0; i < len; i++) {
        dest[i] = source[len - i - 1];
    }
    return dest;
}

number_t ato_base(const char_t* source, number_t base) {
    // TODO: lower upper case chars are same when base<=36
    number_t ret = 0;
    number_t p = 0;

    if(source == NULL) {
        return 0;
    }

    number_t sign = 1;

    if(source[0] == '+') {
        source++;
    }

    if(source[0] == '-') {
        sign = -1;
        source++;
    }

    size_t l = strlen(source);
    for(size_t i = 1; i <= l; i++) {
        if(source[l - i] <= '9') {
            ret += ((number_t)(source[l - i] - 48)) * power(base, p);
        } else {
            ret += ((number_t)(source[l - i] - 55)) * power(base, p);
        }
        p++;
    }
    return sign * ret;
}

unumber_t atou_base(const char_t* source, number_t base) {
    // TODO: lower upper case chars are same when base<=36
    number_t ret = 0;
    number_t p = 0;

    if(source == NULL) {
        return 0;
    }

    size_t l = strlen(source);
    for(size_t i = 1; i <= l; i++) {
        if(source[l - i] <= '9') {
            ret += ((number_t)(source[l - i] - '0')) * power(base, p);
        } else if(source[l - i] <= 'Z') {
            ret += ((number_t)(source[l - i] - 'A') + 10) * power(base, p);
        } else {
            ret += ((number_t)(source[l - i] - 'a') + 10) * power(base, p);
        }
        p++;
    }
    return ret;
}

char_t* ito_base(number_t number, number_t base){
    char_t buf[64];

    memory_memclean(buf, 64);

    if(ito_base_with_buffer(buf, number, base) != 0) {
        return NULL;
    }

    size_t len = strlen(buf);
    char_t* ret = memory_malloc(sizeof(char_t) * len + 1);

    if (ret == NULL) {
        return NULL;
    }

    strcpy(buf, ret);

    return ret;
}

char_t* uto_base(unumber_t number, number_t base){
    char_t buf[64];

    memory_memclean(buf, 64);

    if(uto_base_with_buffer(buf, number, base) != 0) {
        return NULL;
    }

    size_t len = strlen(buf);
    char_t* ret = memory_malloc(sizeof(char_t) * len + 1);

    if (ret == NULL) {
        return NULL;
    }

    strcpy(buf, ret);

    return ret;
}

char_t* strndup_at_heap(memory_heap_t* heap, const char_t* src, size_t l){
    if(src == NULL) {
        return NULL;
    }

    l = MIN(l, strlen(src));

    char_t* res = memory_malloc_ext(heap, sizeof(char_t) * l + 1, 0x0);

    if(res == NULL) {
        return res;
    }

    memory_memcopy(src, res, l);

    return res;
}

int8_t strstarts(const char_t* str, const char_t* prefix) {
    if(strlen(str) < strlen(prefix)) {
        return -1;
    }

    if(memory_memcompare(str, prefix, strlen(prefix)) == 0) {
        return 0;
    }

    return -1;
}

int8_t strends(const char_t* str, const char_t* suffix) {
    if(strlen(str) < strlen(suffix)) {
        return -1;
    }

    if(memory_memcompare(str + strlen(str) - strlen(suffix), suffix, strlen(suffix)) == 0) {
        return 0;
    }

    return -1;
}

char_t* strcat_at_heap(memory_heap_t* heap, const char_t* string1, const char_t* string2) {
    size_t newsize = strlen(string1) + strlen(string2);

    char_t* res = memory_malloc_ext(heap, sizeof(char_t) * newsize + 1, 0x0);

    if(res == NULL) {
        return res;
    }

    strcpy(string1, res);
    strcpy(string2, res + strlen(string1));

    return res;
}

int8_t strncmp(const char_t* string1, const char_t* string2, size_t n) {
    size_t len1 = strlen(string1);
    size_t len2 = strlen(string2);

    size_t minlen_of_strs = MIN(len1, len2);

    if(n <= minlen_of_strs) {
        for(size_t i = 0; i < n; i++) {
            if(string1[i] < string2[i]) {
                return -1;
            }

            if(string1[i] > string2[i]) {
                return 1;
            }
        }

        return 0;
    }

    return strcmp(string1, string2);
}

char_t** strsplit(const char_t* str, const char_t token, int64_t** lengths, int64_t* count) {
    char_t* tmp = (char_t*)str;

    if(count == NULL || lengths == NULL) {
        return NULL;
    }

    *count = 0;

    while(*tmp != NULL) {
        if(*tmp == token) {
            (*count)++;
        }

        tmp++;
    }

    (*count)++;

    *lengths = memory_malloc(sizeof(int64_t) * (*count));

    if(*lengths == NULL) {

        return NULL;
    }

    char_t** result = memory_malloc(sizeof(char_t*) * (*count));

    if(result == NULL) {
        memory_free(*lengths);

        return NULL;
    }

    char_t** result_start = result;

    tmp = (char_t*)str;

    int64_t* tmp_lengths = *lengths;
    int64_t len = 0;

    *result = tmp;

    while(*tmp != NULL) {
        if(*tmp == token) {
            tmp++;

            if(*tmp == NULL) {
                break;
            }

            result++;
            *result = tmp;

            *tmp_lengths = len;
            len = 0;
            tmp_lengths++;

        } else {
            len++;
            tmp++;
        }

    }

    *tmp_lengths = len;


    return result_start;
}


char_t* strupper(char_t* str) {

    if(str == NULL) {
        return NULL;
    }

    int64_t i = 0;

    while(str[i] != NULL) {
        if(str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 0x20;
        }

        i++;
    }

    return str;
}

char_t* struppercopy(const char_t* str) {
    return strupper(strdup(str));
}

char_t* strlower(char_t* str) {

    if(str == NULL) {
        return NULL;
    }

    int64_t i = 0;

    while(str[i] != NULL) {
        if(str[i] >= 'A' && str[i] <= 'Z') {
            str[i] += 0x20;
        }

        i++;
    }

    return str;
}

char_t* strlowercopy(const char_t* str) {
    return strlower(strdup(str));
}

int64_t wchar_size(const wchar_t* str){

    if(str == NULL) {
        return 0;
    }

    int64_t res = 0;
    int64_t i = 0;

    while(str[i++] != 0) {
        res++;
    }

    return res;
}

char_t* wchar_to_char(wchar_t* src){

    if(src == NULL) {
        return NULL;
    }

    int64_t len = wchar_size(src);
    char_t* dst = memory_malloc(sizeof(char_t) * len * 4 + 1);

    if(dst == NULL) {
        return NULL;
    }

    int64_t i = 0;
    int64_t j = 0;

    while(src[i]) {
        if(src[i] >= 0x800) {
            dst[j++] = ((src[i] >> 12) & 0xF) | 0xE0;
            dst[j++] = ((src[i] >> 6) & 0x3F) | 0x80;
            dst[j++] = (src[i] & 0x3F) | 0x80;
        } else if(src[i] >= 0x80) {
            dst[j++] = ((src[i] >> 6) & 0x1F) | 0xC0;
            dst[j++] = (src[i] & 0x3F) | 0x80;
        } else {
            dst[j++] = src[i] & 0x7F;
        }

        i++;
    }

    return dst;
}

wchar_t* char_to_wchar(const char_t* str){

    if(str == NULL) {
        return NULL;
    }

    int64_t len = strlen(str);
    wchar_t* res = memory_malloc(sizeof(wchar_t) * len + 1);

    if(res == NULL) {
        return NULL;
    }

    int64_t i = 0;
    int64_t j = 0;

    while(str[i]) {
        wchar_t wc = str[i];

        if(wc & 128) {
            if((wc & 32) == 0 ) {
                wc = ((str[i] & 0x1F) << 6) + (str[i + 1] & 0x3F);
                i++;
            } else if((wc & 16) == 0 ) {
                wc = ((((str[i] & 0xF) << 6) + (str[i + 1] & 0x3F)) << 6) + (str[i + 2] & 0x3F);
                i += 2;
            } else if((wc & 8) == 0 ) {
                wc = ((((((str[i] & 0x7) << 6) + (str[i + 1] & 0x3F)) << 6) + (str[i + 2] & 0x3F)) << 6) + (str[i + 3] & 0x3F);
                i += 3;
            } else {
                wc = 0;
            }
        }

        res[j] = wc;

        j++;
        i++;
    }

    return res;
}

int64_t lchar_size(const lchar_t* str){
    int64_t res = 0;
    int64_t i = 0;

    while(str[i++] != 0) {
        res++;
    }

    return res;
}

char_t* lchar_to_char(lchar_t* src){

    if(src == NULL) {
        return NULL;
    }

    int64_t len = lchar_size(src);
    char_t* dst = memory_malloc(sizeof(char_t) * len * 4 + 1);

    if(dst == NULL) {
        return NULL;
    }

    int64_t i = 0;
    int64_t j = 0;

    while(src[i]) {
        if(src[i] >= 10000) {
            dst[j++] = ((src[i] >> 18) & 0x7) | 0xF0;
            dst[j++] = ((src[i] >> 12) & 0x3F) | 0x80;
            dst[j++] = ((src[i] >> 6) & 0x3F) | 0x80;
            dst[j++] = (src[i] & 0x3F) | 0x80;
        } else if(src[i] >= 0x800) {
            dst[j++] = ((src[i] >> 12) & 0xF) | 0xE0;
            dst[j++] = ((src[i] >> 6) & 0x3F) | 0x80;
            dst[j++] = (src[i] & 0x3F) | 0x80;
        } else if(src[i] >= 0x80) {
            dst[j++] = ((src[i] >> 6) & 0x1F) | 0xC0;
            dst[j++] = (src[i] & 0x3F) | 0x80;
        } else {
            dst[j++] = src[i] & 0x7F;
        }

        i++;
    }

    return dst;
}

lchar_t* char_to_lchar(char_t* str){

    if(str == NULL) {
        return NULL;
    }

    int64_t len = strlen(str);
    lchar_t* res = memory_malloc(sizeof(wchar_t) * len + 1);

    if(res == NULL) {
        return NULL;
    }

    int64_t i = 0;
    int64_t j = 0;

    while(str[i]) {
        wchar_t wc = str[i];

        if(wc & 128) {
            if((wc & 32) == 0 ) {
                wc = ((str[i] & 0x1F) << 6) + (str[i + 1] & 0x3F);
                i++;
            } else if((wc & 16) == 0 ) {
                wc = ((((str[i] & 0xF) << 6) + (str[i + 1] & 0x3F)) << 6) + (str[i + 2] & 0x3F);
                i += 2;
            } else if((wc & 8) == 0 ) {
                wc = ((((((str[i] & 0x7) << 6) + (str[i + 1] & 0x3F)) << 6) + (str[i + 2] & 0x3F)) << 6) + (str[i + 3] & 0x3F);
                i += 3;
            } else {
                wc = 0;
            }
        }

        res[j] = wc;

        j++;
        i++;
    }

    return res;
}

char_t* strtrim_right(char_t* str) {

    if(str == NULL) {
        return NULL;
    }

    for(int64_t i = strlen(str) - 1; i >= 0; i--) {
        if(str[i] == '\n' || str[i] == '\r' || str[i] == '\t' || str[i] == ' ') {
            str[i] = NULL;
        } else {
            break;
        }
    }

    return str;
}

int8_t str_is_upper(char_t* str) {
    if(str == NULL) {
        return 0;
    }

    int64_t i = 0;

    while(str[i] != NULL) {
        if(str[i] >= 'a' && str[i] <= 'z') {
            return -1;
        }

        i++;
    }

    return 0;
}

uint64_t strhash(const char_t* input) {

    if(input == NULL) {
        return NULL;
    }

    char_t* tmp = (char_t*)input;

    if(tmp == NULL) {
        return 0;
    }

    uint64_t res = 5381;

    while(tmp) {
        res = ((res << 5) + res) + *tmp;
        tmp++;
    }

    return res;
}
