/**
 * @file aes.64.c
 * @brief AES encryption and decryption functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <aes.h>
#include <memory.h>

MODULE("turnstone.lib.crypto");

static int32_t aes_tables_inited = 0;


static uint8_t AES_FSb[256] = {0};
static uint32_t AES_FT0[256] = {0};
static uint32_t AES_FT1[256] = {0};
static uint32_t AES_FT2[256] = {0};
static uint32_t AES_FT3[256] = {0};

static uint8_t AES_RSb[256] = {0};
static uint32_t AES_RT0[256] = {0};
static uint32_t AES_RT1[256] = {0};
static uint32_t AES_RT2[256] = {0};
static uint32_t AES_RT3[256] = {0};

static uint32_t AES_RCON[10] = {0};


int32_t aes_set_encryption_key(aes_context_t * ctx, const uint8_t * key, uint32_t keysize);
int32_t aes_set_decryption_key(aes_context_t * ctx, const uint8_t * key, uint32_t keysize);

#define GET_UINT32_LE(n, b, i) {                  \
            (n) = ( (uint32_t) (b)[(i)    ]       )     \
                  | ( (uint32_t) (b)[(i) + 1] <<  8 )     \
                  | ( (uint32_t) (b)[(i) + 2] << 16 )     \
                  | ( (uint32_t) (b)[(i) + 3] << 24 ); }

#define PUT_UINT32_LE(n, b, i) {                  \
            (b)[(i)    ] = (uint8_t) ( (n)       );       \
            (b)[(i) + 1] = (uint8_t) ( (n) >>  8 );       \
            (b)[(i) + 2] = (uint8_t) ( (n) >> 16 );       \
            (b)[(i) + 3] = (uint8_t) ( (n) >> 24 ); }

#define AES_FROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3)     \
        {                                               \
            X0 = *RK++ ^ AES_FT0[ ( Y0       ) & 0xFF ] ^   \
                 AES_FT1[ ( Y1 >>  8 ) & 0xFF ] ^   \
                 AES_FT2[ ( Y2 >> 16 ) & 0xFF ] ^   \
                 AES_FT3[ ( Y3 >> 24 ) & 0xFF ];    \
                                                \
            X1 = *RK++ ^ AES_FT0[ ( Y1       ) & 0xFF ] ^   \
                 AES_FT1[ ( Y2 >>  8 ) & 0xFF ] ^   \
                 AES_FT2[ ( Y3 >> 16 ) & 0xFF ] ^   \
                 AES_FT3[ ( Y0 >> 24 ) & 0xFF ];    \
                                                \
            X2 = *RK++ ^ AES_FT0[ ( Y2       ) & 0xFF ] ^   \
                 AES_FT1[ ( Y3 >>  8 ) & 0xFF ] ^   \
                 AES_FT2[ ( Y0 >> 16 ) & 0xFF ] ^   \
                 AES_FT3[ ( Y1 >> 24 ) & 0xFF ];    \
                                                \
            X3 = *RK++ ^ AES_FT0[ ( Y3       ) & 0xFF ] ^   \
                 AES_FT1[ ( Y0 >>  8 ) & 0xFF ] ^   \
                 AES_FT2[ ( Y1 >> 16 ) & 0xFF ] ^   \
                 AES_FT3[ ( Y2 >> 24 ) & 0xFF ];    \
        }

#define AES_RROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3)     \
        {                                               \
            X0 = *RK++ ^ AES_RT0[ ( Y0       ) & 0xFF ] ^   \
                 AES_RT1[ ( Y3 >>  8 ) & 0xFF ] ^   \
                 AES_RT2[ ( Y2 >> 16 ) & 0xFF ] ^   \
                 AES_RT3[ ( Y1 >> 24 ) & 0xFF ];    \
                                                \
            X1 = *RK++ ^ AES_RT0[ ( Y1       ) & 0xFF ] ^   \
                 AES_RT1[ ( Y0 >>  8 ) & 0xFF ] ^   \
                 AES_RT2[ ( Y3 >> 16 ) & 0xFF ] ^   \
                 AES_RT3[ ( Y2 >> 24 ) & 0xFF ];    \
                                                \
            X2 = *RK++ ^ AES_RT0[ ( Y2       ) & 0xFF ] ^   \
                 AES_RT1[ ( Y1 >>  8 ) & 0xFF ] ^   \
                 AES_RT2[ ( Y0 >> 16 ) & 0xFF ] ^   \
                 AES_RT3[ ( Y3 >> 24 ) & 0xFF ];    \
                                                \
            X3 = *RK++ ^ AES_RT0[ ( Y3       ) & 0xFF ] ^   \
                 AES_RT1[ ( Y2 >>  8 ) & 0xFF ] ^   \
                 AES_RT2[ ( Y1 >> 16 ) & 0xFF ] ^   \
                 AES_RT3[ ( Y0 >> 24 ) & 0xFF ];    \
        }

#define ROTL8(x) ( ( x << 8 ) & 0xFFFFFFFF ) | ( x >> 24 )
#define XTIME(x) ( ( x << 1 ) ^ ( ( x & 0x80 ) ? 0x1B : 0x00 ) )
#define MUL(x, y) ( ( x && y ) ? pow[(log[x] + log[y]) % 255] : 0 )
#define MIX(x, y) { y = ( (y << 1) | (y >> 7) ) & 0xFF; x ^= y; }
#define CPY128   { *RK++ = *SK++; *RK++ = *SK++; \
                   *RK++ = *SK++; *RK++ = *SK++; }

void aes_init_keygen_tables(void) {
    int32_t i, x, y, z;
    int32_t pow[256];
    int32_t log[256];

    if (aes_tables_inited) {
        return;
    }

    for( i = 0, x = 1; i < 256; i++ )   {
        pow[i] = x;
        log[x] = i;
        x = ( x ^ XTIME( x ) ) & 0xFF;
    }

    for( i = 0, x = 1; i < 10; i++ )    {
        AES_RCON[i] = (uint32_t) x;
        x = XTIME( x ) & 0xFF;
    }

    AES_FSb[0x00] = 0x63;
    AES_RSb[0x63] = 0x00;

    for( i = 1; i < 256; i++ )          {
        x = y = pow[255 - log[i]];
        MIX(x, y);
        MIX(x, y);
        MIX(x, y);
        MIX(x, y);
        AES_FSb[i] = (uint8_t) ( x ^= 0x63 );
        AES_RSb[x] = (uint8_t) i;
    }

    for( i = 0; i < 256; i++ )          {
        x = AES_FSb[i];
        y = XTIME( x ) & 0xFF;
        z =  ( y ^ x ) & 0xFF;

        AES_FT0[i] = ( (uint32_t) y       ) ^ ( (uint32_t) x <<  8 ) ^
                     ( (uint32_t) x << 16 ) ^ ( (uint32_t) z << 24 );

        AES_FT1[i] = ROTL8( AES_FT0[i] );
        AES_FT2[i] = ROTL8( AES_FT1[i] );
        AES_FT3[i] = ROTL8( AES_FT2[i] );

        x = AES_RSb[i];

        AES_RT0[i] = ( (uint32_t) MUL( 0x0E, x )       ) ^
                     ( (uint32_t) MUL( 0x09, x ) <<  8 ) ^
                     ( (uint32_t) MUL( 0x0D, x ) << 16 ) ^
                     ( (uint32_t) MUL( 0x0B, x ) << 24 );

        AES_RT1[i] = ROTL8( AES_RT0[i] );
        AES_RT2[i] = ROTL8( AES_RT1[i] );
        AES_RT3[i] = ROTL8( AES_RT2[i] );
    }

    aes_tables_inited = 1;
}

int32_t aes_set_encryption_key(aes_context_t * ctx, const uint8_t * key, uint32_t keysize) {
    uint32_t i;
    uint32_t * RK = ctx->rk;

    for( i = 0; i < (keysize >> 2); i++ ) {
        GET_UINT32_LE( RK[i], key, i << 2 );
    }

    switch( ctx->rounds ) {
    case 10:
        for( i = 0; i < 10; i++, RK += 4 ) {
            RK[4]  = RK[0] ^ AES_RCON[i] ^
                     ( (uint32_t) AES_FSb[ ( RK[3] >>  8 ) & 0xFF ]       ) ^
                     ( (uint32_t) AES_FSb[ ( RK[3] >> 16 ) & 0xFF ] <<  8 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[3] >> 24 ) & 0xFF ] << 16 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[3]       ) & 0xFF ] << 24 );

            RK[5]  = RK[1] ^ RK[4];
            RK[6]  = RK[2] ^ RK[5];
            RK[7]  = RK[3] ^ RK[6];
        }
        break;

    case 12:
        for( i = 0; i < 8; i++, RK += 6 ) {
            RK[6]  = RK[0] ^ AES_RCON[i] ^
                     ( (uint32_t) AES_FSb[ ( RK[5] >>  8 ) & 0xFF ]       ) ^
                     ( (uint32_t) AES_FSb[ ( RK[5] >> 16 ) & 0xFF ] <<  8 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[5] >> 24 ) & 0xFF ] << 16 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[5]       ) & 0xFF ] << 24 );

            RK[7]  = RK[1] ^ RK[6];
            RK[8]  = RK[2] ^ RK[7];
            RK[9]  = RK[3] ^ RK[8];
            RK[10] = RK[4] ^ RK[9];
            RK[11] = RK[5] ^ RK[10];
        }
        break;

    case 14:
        for( i = 0; i < 7; i++, RK += 8 ) {
            RK[8]  = RK[0] ^ AES_RCON[i] ^
                     ( (uint32_t) AES_FSb[ ( RK[7] >>  8 ) & 0xFF ]       ) ^
                     ( (uint32_t) AES_FSb[ ( RK[7] >> 16 ) & 0xFF ] <<  8 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[7] >> 24 ) & 0xFF ] << 16 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[7]       ) & 0xFF ] << 24 );

            RK[9]  = RK[1] ^ RK[8];
            RK[10] = RK[2] ^ RK[9];
            RK[11] = RK[3] ^ RK[10];

            RK[12] = RK[4] ^
                     ( (uint32_t) AES_FSb[ ( RK[11]       ) & 0xFF ]       ) ^
                     ( (uint32_t) AES_FSb[ ( RK[11] >>  8 ) & 0xFF ] <<  8 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[11] >> 16 ) & 0xFF ] << 16 ) ^
                     ( (uint32_t) AES_FSb[ ( RK[11] >> 24 ) & 0xFF ] << 24 );

            RK[13] = RK[5] ^ RK[12];
            RK[14] = RK[6] ^ RK[13];
            RK[15] = RK[7] ^ RK[14];
        }
        break;

    default:
        return -1;
    }

    return 0;
}

int32_t aes_set_decryption_key(aes_context_t * ctx, const uint8_t * key, uint32_t keysize) {
    int32_t i, j;
    aes_context_t cty;
    uint32_t * RK = ctx->rk;
    uint32_t * SK;
    int32_t ret;

    cty.rounds = ctx->rounds;
    cty.rk = cty.buf;

    ret = aes_set_encryption_key( &cty, key, keysize );

    if (ret != 0 )
        return ret;

    SK = cty.rk + cty.rounds * 4;

    CPY128;

    for( i = ctx->rounds - 1, SK -= 8; i > 0; i--, SK -= 8 ) {
        for( j = 0; j < 4; j++, SK++ ) {
            *RK++ = AES_RT0[ AES_FSb[ ( *SK       ) & 0xFF ] ] ^
                    AES_RT1[ AES_FSb[ ( *SK >>  8 ) & 0xFF ] ] ^
                    AES_RT2[ AES_FSb[ ( *SK >> 16 ) & 0xFF ] ] ^
                    AES_RT3[ AES_FSb[ ( *SK >> 24 ) & 0xFF ] ];
        }
    }

    CPY128;
    memory_memset( &cty, 0, sizeof(aes_context_t));

    return 0;
}

int32_t aes_setkey(aes_context_t * ctx, int32_t mode, const uint8_t * key, uint32_t keysize ){
    if(!aes_tables_inited) {
        return ( -1 );
    }

    ctx->mode = mode;
    ctx->rk = ctx->buf;

    switch( keysize )
    {
    case 16: ctx->rounds = 10; break;
    case 24: ctx->rounds = 12; break;
    case 32: ctx->rounds = 14; break;
    default: return -1;
    }

    if( mode == AES_DECRYPT ) {
        return aes_set_decryption_key( ctx, key, keysize);
    }

    return aes_set_encryption_key( ctx, key, keysize);
}

int32_t aes_cipher( aes_context_t * ctx,
                    const uint8_t   input[16],
                    uint8_t         output[16] )
{
    int32_t i;
    uint32_t * RK, X0, X1, X2, X3, Y0, Y1, Y2, Y3;

    RK = ctx->rk;

    GET_UINT32_LE( X0, input,  0 ); X0 ^= *RK++;
    GET_UINT32_LE( X1, input,  4 ); X1 ^= *RK++;
    GET_UINT32_LE( X2, input,  8 ); X2 ^= *RK++;
    GET_UINT32_LE( X3, input, 12 ); X3 ^= *RK++;

    if( ctx->mode == AES_DECRYPT ) {
        for( i = (ctx->rounds >> 1) - 1; i > 0; i-- ) {
            AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );
            AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );
        }

        AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );

        X0 = *RK++ ^ \
             ( (uint32_t) AES_RSb[ ( Y0       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_RSb[ ( Y3 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_RSb[ ( Y2 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_RSb[ ( Y1 >> 24 ) & 0xFF ] << 24 );

        X1 = *RK++ ^ \
             ( (uint32_t) AES_RSb[ ( Y1       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_RSb[ ( Y0 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_RSb[ ( Y3 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_RSb[ ( Y2 >> 24 ) & 0xFF ] << 24 );

        X2 = *RK++ ^ \
             ( (uint32_t) AES_RSb[ ( Y2       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_RSb[ ( Y1 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_RSb[ ( Y0 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_RSb[ ( Y3 >> 24 ) & 0xFF ] << 24 );

        X3 = *RK++ ^ \
             ( (uint32_t) AES_RSb[ ( Y3       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_RSb[ ( Y2 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_RSb[ ( Y1 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_RSb[ ( Y0 >> 24 ) & 0xFF ] << 24 );
    }
    else {

        for( i = (ctx->rounds >> 1) - 1; i > 0; i-- ) {
            AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );
            AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );
        }

        AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );

        X0 = *RK++ ^ \
             ( (uint32_t) AES_FSb[ ( Y0       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_FSb[ ( Y1 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_FSb[ ( Y2 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_FSb[ ( Y3 >> 24 ) & 0xFF ] << 24 );

        X1 = *RK++ ^ \
             ( (uint32_t) AES_FSb[ ( Y1       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_FSb[ ( Y2 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_FSb[ ( Y3 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_FSb[ ( Y0 >> 24 ) & 0xFF ] << 24 );

        X2 = *RK++ ^ \
             ( (uint32_t) AES_FSb[ ( Y2       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_FSb[ ( Y3 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_FSb[ ( Y0 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_FSb[ ( Y1 >> 24 ) & 0xFF ] << 24 );

        X3 = *RK++ ^ \
             ( (uint32_t) AES_FSb[ ( Y3       ) & 0xFF ]       ) ^
             ( (uint32_t) AES_FSb[ ( Y0 >>  8 ) & 0xFF ] <<  8 ) ^
             ( (uint32_t) AES_FSb[ ( Y1 >> 16 ) & 0xFF ] << 16 ) ^
             ( (uint32_t) AES_FSb[ ( Y2 >> 24 ) & 0xFF ] << 24 );

    }

    PUT_UINT32_LE( X0, output,  0 );
    PUT_UINT32_LE( X1, output,  4 );
    PUT_UINT32_LE( X2, output,  8 );
    PUT_UINT32_LE( X3, output, 12 );

    return 0;
}
/* end of aes.c */
