/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crc.h>

MODULE("turnstone.lib");


uint32_t crc32_table[256] = {};

void crc32_init_table(void) {
    uint8_t index = 0, z;
    do{
        crc32_table[index] = index;
        for(z = 8; z; z--) {
            crc32_table[index] = (crc32_table[index] & 1)?((crc32_table[index] >> 1) ^ 0xEDB88320):(crc32_table[index] >> 1);
        }
    }while(++index);
}

uint32_t crc32_sum(uint8_t* p, uint32_t bytelength, uint32_t init) {
    uint32_t crc = init;
    while (bytelength-- != 0) {
        crc = crc32_table[((uint8_t) crc ^ *(p++))] ^ (crc >> 8);
    }
    return crc;
}
