/**
 * @file gcm.64.c
 * @brief Galois/Counter Mode (GCM) implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <gcm.h>
#include <aes.h>
#include <memory.h>

MODULE("turnstone.lib.crypto");

static const uint64_t gcm_last4[16] = {
    0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

#define GET_UINT32_BE(n, b, i) {                      \
            (n) = ( (uint32_t) (b)[(i)    ] << 24 )         \
                  | ( (uint32_t) (b)[(i) + 1] << 16 )         \
                  | ( (uint32_t) (b)[(i) + 2] <<  8 )         \
                  | ( (uint32_t) (b)[(i) + 3]       ); }

#define PUT_UINT32_BE(n, b, i) {                      \
            (b)[(i)    ] = (uint8_t) ( (n) >> 24 );   \
            (b)[(i) + 1] = (uint8_t) ( (n) >> 16 );   \
            (b)[(i) + 2] = (uint8_t) ( (n) >>  8 );   \
            (b)[(i) + 3] = (uint8_t) ( (n)       ); }



int32_t gcm_initialize(void) {
    aes_init_keygen_tables();
    return 0;
}

static void gcm_mult(gcm_context_t * ctx, const uint8_t x[16], uint8_t output[16]) {
    int32_t i;
    uint8_t lo, hi, rem;
    uint64_t zh, zl;

    lo = (uint8_t)( x[15] & 0x0f );
    hi = (uint8_t)( x[15] >> 4 );
    zh = ctx->HH[lo];
    zl = ctx->HL[lo];

    for(i = 15; i >= 0; i--) {
        lo = (uint8_t) (x[i] & 0x0f);
        hi = (uint8_t) (x[i] >> 4);

        if(i != 15) {
            rem = (uint8_t) ( zl & 0x0f );
            zl = ( zh << 60 ) | ( zl >> 4 );
            zh = ( zh >> 4 );
            zh ^= (uint64_t) gcm_last4[rem] << 48;
            zh ^= ctx->HH[lo];
            zl ^= ctx->HL[lo];
        }

        rem = (uint8_t) ( zl & 0x0f );
        zl = ( zh << 60 ) | ( zl >> 4 );
        zh = ( zh >> 4 );
        zh ^= (uint64_t) gcm_last4[rem] << 48;
        zh ^= ctx->HH[hi];
        zl ^= ctx->HL[hi];
    }

    PUT_UINT32_BE( zh >> 32, output, 0 );
    PUT_UINT32_BE( zh, output, 4 );
    PUT_UINT32_BE( zl >> 32, output, 8 );
    PUT_UINT32_BE( zl, output, 12 );
}


int32_t gcm_setkey(gcm_context_t * ctx, const uint8_t * key, const uint32_t keysize){
    int32_t ret, i, j;
    uint64_t hi, lo;
    uint64_t vl, vh;
    unsigned char h[16];

    memory_memset( ctx, 0, sizeof(gcm_context_t) );
    memory_memset( h, 0, 16 );

    ret = aes_setkey( &ctx->aes_ctx, AES_ENCRYPT, key, keysize );

    if(ret != 0 ) {
        return ret;
    }

    ret = aes_cipher( &ctx->aes_ctx, h, h );

    if(ret != 0) {
        return ret;
    }

    GET_UINT32_BE( hi, h,  0  );
    GET_UINT32_BE( lo, h,  4  );
    vh = (uint64_t) hi << 32 | lo;

    GET_UINT32_BE( hi, h,  8  );
    GET_UINT32_BE( lo, h,  12 );
    vl = (uint64_t) hi << 32 | lo;

    ctx->HL[8] = vl;
    ctx->HH[8] = vh;
    ctx->HH[0] = 0;
    ctx->HL[0] = 0;

    for(i = 4; i > 0; i >>= 1) {
        uint32_t T = (uint32_t) ( vl & 1 ) * 0xe1000000U;
        vl  = ( vh << 63 ) | ( vl >> 1 );
        vh  = ( vh >> 1 ) ^ ( (uint64_t) T << 32);
        ctx->HL[i] = vl;
        ctx->HH[i] = vh;
    }

    for(i = 2; i < 16; i <<= 1) {
        uint64_t * HiL = ctx->HL + i, * HiH = ctx->HH + i;
        vh = *HiH;
        vl = *HiL;

        for(j = 1; j < i; j++) {
            HiH[j] = vh ^ ctx->HH[j];
            HiL[j] = vl ^ ctx->HL[j];
        }
    }

    return 0;
}

int32_t gcm_start(gcm_context_t * ctx, int32_t mode, const uint8_t * iv, size_t iv_len, const uint8_t * add, size_t add_len){
    int32_t ret;
    uint8_t work_buf[16];
    const uint8_t * p;
    size_t use_len;
    size_t i;



    memory_memset( ctx->y,   0x00, sizeof(ctx->y  ) );
    memory_memset( ctx->buf, 0x00, sizeof(ctx->buf) );
    ctx->len = 0;
    ctx->add_len = 0;

    ctx->mode = mode;
    ctx->aes_ctx.mode = AES_ENCRYPT;

    if( iv_len == 12 ) {
        memory_memcopy(iv, ctx->y, iv_len );
        ctx->y[15] = 1;
    } else {
        memory_memset( work_buf, 0x00, 16 );
        PUT_UINT32_BE( iv_len * 8, work_buf, 12 );

        p = iv;

        while( iv_len > 0 ) {
            use_len = ( iv_len < 16 ) ? iv_len : 16;
            for( i = 0; i < use_len; i++ ) ctx->y[i] ^= p[i];
            gcm_mult( ctx, ctx->y, ctx->y );
            iv_len -= use_len;
            p += use_len;
        }

        for( i = 0; i < 16; i++ ) {
            ctx->y[i] ^= work_buf[i];
        }

        gcm_mult( ctx, ctx->y, ctx->y );
    }

    ret = aes_cipher( &ctx->aes_ctx, ctx->y, ctx->base_ectr);

    if( ret != 0 ) {
        return ret;
    }

    ctx->add_len = add_len;
    p = add;

    while( add_len > 0 ) {
        use_len = ( add_len < 16 ) ? add_len : 16;

        for( i = 0; i < use_len; i++ ) {
            ctx->buf[i] ^= p[i];
        }

        gcm_mult( ctx, ctx->buf, ctx->buf );
        add_len -= use_len;
        p += use_len;
    }

    return 0;
}

int32_t gcm_update(gcm_context_t * ctx, size_t length, const uint8_t * input, uint8_t* output) {
    int32_t ret;
    uint8_t ectr[16];
    size_t use_len;
    size_t i;

    ctx->len += length;

    while( length > 0 ) {
        use_len = ( length < 16 ) ? length : 16;

        for( i = 16; i > 12; i-- ) {
            if( ++ctx->y[i - 1] != 0 ) {
                break;
            }
        }

        ret = aes_cipher( &ctx->aes_ctx, ctx->y, ectr );

        if( ret != 0 ) {
            return ret;
        }

        if( ctx->mode == AES_ENCRYPT ) {
            for( i = 0; i < use_len; i++ ) {
                output[i] = (uint8_t) ( ectr[i] ^ input[i] );
                ctx->buf[i] ^= output[i];
            }
        } else {
            for( i = 0; i < use_len; i++ ) {
                ctx->buf[i] ^= input[i];
                output[i] = (uint8_t) ( ectr[i] ^ input[i] );
            }
        }

        gcm_mult( ctx, ctx->buf, ctx->buf );

        length -= use_len;
        input  += use_len;
        output += use_len;
    }

    return 0;
}

int32_t gcm_finish(gcm_context_t* ctx, uint8_t* tag, size_t tag_len) {
    uint8_t work_buf[16];
    uint64_t orig_len     = ctx->len * 8;
    uint64_t orig_add_len = ctx->add_len * 8;
    size_t i;

    if(tag_len != 0) {
        memory_memcopy( ctx->base_ectr, tag, tag_len );
    }

    if( orig_len || orig_add_len ) {
        memory_memset( work_buf, 0x00, 16 );

        PUT_UINT32_BE( ( orig_add_len >> 32 ), work_buf, 0  );
        PUT_UINT32_BE( ( orig_add_len       ), work_buf, 4  );
        PUT_UINT32_BE( ( orig_len     >> 32 ), work_buf, 8  );
        PUT_UINT32_BE( ( orig_len           ), work_buf, 12 );

        for( i = 0; i < 16; i++ ) {
            ctx->buf[i] ^= work_buf[i];
        }

        gcm_mult( ctx, ctx->buf, ctx->buf );

        for( i = 0; i < tag_len; i++ ) {
            tag[i] ^= ctx->buf[i];
        }
    }

    return 0;
}

int32_t gcm_crypt_and_tag(gcm_context_t * ctx, int32_t mode, const uint8_t * iv, size_t iv_len, const uint8_t * add, size_t add_len, const uint8_t * input, uint8_t* output, size_t length, uint8_t* tag, size_t tag_len) {
    gcm_start(ctx, mode, iv, iv_len, add, add_len);
    gcm_update(ctx, length, input, output);
    gcm_finish(ctx, tag, tag_len);

    return 0;
}

int32_t gcm_auth_decrypt(gcm_context_t * ctx, const uint8_t* iv, size_t iv_len, const uint8_t* add, size_t add_len, const uint8_t* input, uint8_t* output, size_t length, const uint8_t* tag, size_t tag_len) {
    uint8_t check_tag[16];
    int32_t diff;
    size_t i;

    gcm_crypt_and_tag(ctx, AES_DECRYPT, iv, iv_len, add, add_len,
                      input, output, length, check_tag, tag_len);


    for( diff = 0, i = 0; i < tag_len; i++ ) {
        diff |= tag[i] ^ check_tag[i];
    }

    if( diff != 0 ) {
        memory_memset(output, 0, length);

        return GCM_AUTH_FAILURE;
    }

    return 0;
}

void gcm_zero_ctx(gcm_context_t * ctx) {
    memory_memset(ctx, 0, sizeof(gcm_context_t));
}
