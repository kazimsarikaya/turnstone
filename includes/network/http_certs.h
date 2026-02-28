/**
 * @file http_certs.h
 * @brief HTTP certificates extern declarations.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___HTTP_CERTS_H
#define ___HTTP_CERTS_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t http_ca_priv_data_start[];
extern uint8_t http_ca_priv_data_end[];
extern uint8_t http_ca_pub_data_start[];
extern uint8_t http_ca_pub_data_end[];
extern char_t http_ca_pem_pub_data_start[];
extern char_t http_ca_pem_pub_data_end[];

#ifdef __cplusplus
}
#endif

#endif
