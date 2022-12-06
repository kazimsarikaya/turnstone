/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <utils.h>
#include <memory.h>
#include <varint.h>


uint8_t* varint_encode(uint64_t num, int8_t* size) {
    const uint64_t max = 128;
    uint8_t* tmp_data = memory_malloc(sizeof(uint8_t)*10);
    uint8_t* tmp_start_data = tmp_data;
    int8_t tmp_size = 0;

    tmp_data += 9;

    while(1){
        tmp_size++;

        if(num<max){
            *tmp_data = num | (tmp_size>1?0x80:0x0);
            break;
        } else {
            *tmp_data = (num & 0x7F) | (tmp_size>1?0x80:0x0);
            num >>= 7;
            tmp_data--;
        }       
    }

    (*size) = tmp_size;

    uint8_t* res = memory_malloc(sizeof(uint8_t)*tmp_size);

    memory_memcopy(tmp_data,res,tmp_size);

    memory_free(tmp_start_data);

    return res;
}

uint64_t varint_decode(uint8_t* data, int8_t* size) {
   uint64_t res = 0;

   res = *data & 0x7F;
   (*size) = 1;

   while(*data & 0x80){
       data++;
       (*size)++;

       res <<= 7;
       res |= *data & 0x7F;
   }

   return res;
}