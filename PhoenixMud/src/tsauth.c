/* tsauth.c  -  scrypt verification of TypeScript-platform password hashes.
 * See tsauth.h for why this is vendored rather than linked against OpenSSL.
 *
 * Implemented from the specifications: FIPS 180-4 (SHA-256), RFC 2104 (HMAC),
 * RFC 8018 section 5.2 (PBKDF2), RFC 7914 (Salsa20/8 core, scryptBlockMix,
 * scryptROMix, scrypt).
 *
 * Portability notes that matter for this build:
 *   - compiles as C89-with-stdint under `gcc -m32`;
 *   - all word<->byte conversion is explicit, so it is endian-independent;
 *   - the large allocation is bounded by N*r*128 and checked, because these
 *     parameters come from a stored credential and must not be trusted to be
 *     sane.
 */

#include "tsauth.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -- SHA-256 (FIPS 180-4) ------------------------------------------------- */

typedef struct {
    uint32_t h[8];
    uint64_t len;          /* message length in BYTES */
    unsigned char buf[64];
    size_t buflen;
} sha256_ctx;

static const uint32_t K256[64] = {
    0x428a2f98UL,0x71374491UL,0xb5c0fbcfUL,0xe9b5dba5UL,0x3956c25bUL,0x59f111f1UL,
    0x923f82a4UL,0xab1c5ed5UL,0xd807aa98UL,0x12835b01UL,0x243185beUL,0x550c7dc3UL,
    0x72be5d74UL,0x80deb1feUL,0x9bdc06a7UL,0xc19bf174UL,0xe49b69c1UL,0xefbe4786UL,
    0x0fc19dc6UL,0x240ca1ccUL,0x2de92c6fUL,0x4a7484aaUL,0x5cb0a9dcUL,0x76f988daUL,
    0x983e5152UL,0xa831c66dUL,0xb00327c8UL,0xbf597fc7UL,0xc6e00bf3UL,0xd5a79147UL,
    0x06ca6351UL,0x14292967UL,0x27b70a85UL,0x2e1b2138UL,0x4d2c6dfcUL,0x53380d13UL,
    0x650a7354UL,0x766a0abbUL,0x81c2c92eUL,0x92722c85UL,0xa2bfe8a1UL,0xa81a664bUL,
    0xc24b8b70UL,0xc76c51a3UL,0xd192e819UL,0xd6990624UL,0xf40e3585UL,0x106aa070UL,
    0x19a4c116UL,0x1e376c08UL,0x2748774cUL,0x34b0bcb5UL,0x391c0cb3UL,0x4ed8aa4aUL,
    0x5b9cca4fUL,0x682e6ff3UL,0x748f82eeUL,0x78a5636fUL,0x84c87814UL,0x8cc70208UL,
    0x90befffaUL,0xa4506cebUL,0xbef9a3f7UL,0xc67178f2UL
};

#define ROTR32(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(sha256_ctx *c, const unsigned char *p)
{
    uint32_t w[64], a, b, cc, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16)
             | ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTR32(w[i-15],7) ^ ROTR32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROTR32(w[i-2],17) ^ ROTR32(w[i-2],19)  ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3];
    e=c->h[4]; f=c->h[5]; g=c->h[6];  h=c->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ROTR32(e,6) ^ ROTR32(e,11) ^ ROTR32(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t S0 = ROTR32(a,2) ^ ROTR32(a,13) ^ ROTR32(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        t1 = h + S1 + ch + K256[i] + w[i];
        t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g;  c->h[7]+=h;
}

static void sha256_init(sha256_ctx *c)
{
    c->h[0]=0x6a09e667UL; c->h[1]=0xbb67ae85UL; c->h[2]=0x3c6ef372UL; c->h[3]=0xa54ff53aUL;
    c->h[4]=0x510e527fUL; c->h[5]=0x9b05688cUL; c->h[6]=0x1f83d9abUL; c->h[7]=0x5be0cd19UL;
    c->len = 0; c->buflen = 0;
}

static void sha256_update(sha256_ctx *c, const unsigned char *p, size_t n)
{
    c->len += n;
    while (n > 0) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take; p += take; n -= take;
        if (c->buflen == 64) { sha256_block(c, c->buf); c->buflen = 0; }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char out[32])
{
    uint64_t bits = c->len * 8;
    unsigned char pad = 0x80;
    unsigned char lenbe[8];
    int i;
    sha256_update(c, &pad, 1);
    { unsigned char z = 0; while (c->buflen != 56) sha256_update(c, &z, 1); }
    for (i = 0; i < 8; i++) lenbe[i] = (unsigned char)(bits >> (56 - 8*i));
    sha256_update(c, lenbe, 8);
    for (i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(c->h[i] >> 24);
        out[i*4+1] = (unsigned char)(c->h[i] >> 16);
        out[i*4+2] = (unsigned char)(c->h[i] >> 8);
        out[i*4+3] = (unsigned char)(c->h[i]);
    }
}

/* -- HMAC-SHA256 (RFC 2104) ----------------------------------------------- */

typedef struct { sha256_ctx inner; unsigned char okey[64]; } hmac_ctx;

static void hmac_init(hmac_ctx *h, const unsigned char *key, size_t keylen)
{
    unsigned char ikey[64], k[32];
    int i;
    memset(ikey, 0, 64); memset(h->okey, 0, 64);
    if (keylen > 64) {
        sha256_ctx t; sha256_init(&t); sha256_update(&t, key, keylen); sha256_final(&t, k);
        memcpy(ikey, k, 32); memcpy(h->okey, k, 32);
    } else {
        memcpy(ikey, key, keylen); memcpy(h->okey, key, keylen);
    }
    for (i = 0; i < 64; i++) { ikey[i] ^= 0x36; h->okey[i] ^= 0x5c; }
    sha256_init(&h->inner);
    sha256_update(&h->inner, ikey, 64);
}

static void hmac_update(hmac_ctx *h, const unsigned char *p, size_t n)
{ sha256_update(&h->inner, p, n); }

static void hmac_final(hmac_ctx *h, unsigned char out[32])
{
    unsigned char ih[32];
    sha256_ctx o;
    sha256_final(&h->inner, ih);
    sha256_init(&o);
    sha256_update(&o, h->okey, 64);
    sha256_update(&o, ih, 32);
    sha256_final(&o, out);
}

/* -- PBKDF2-HMAC-SHA256 (RFC 8018 section 5.2) ---------------------------------- */

static void pbkdf2_sha256(const unsigned char *pass, size_t passlen,
                          const unsigned char *salt, size_t saltlen,
                          unsigned long iter, unsigned char *out, size_t outlen)
{
    unsigned char u[32], t[32], idx[4];
    unsigned long i;
    size_t done = 0, blk = 1, j, k;
    while (done < outlen) {
        hmac_ctx h;
        size_t take;
        idx[0]=(unsigned char)(blk>>24); idx[1]=(unsigned char)(blk>>16);
        idx[2]=(unsigned char)(blk>>8);  idx[3]=(unsigned char)(blk);
        hmac_init(&h, pass, passlen);
        hmac_update(&h, salt, saltlen);
        hmac_update(&h, idx, 4);
        hmac_final(&h, u);
        memcpy(t, u, 32);
        for (i = 1; i < iter; i++) {
            hmac_init(&h, pass, passlen);
            hmac_update(&h, u, 32);
            hmac_final(&h, u);
            for (j = 0; j < 32; j++) t[j] ^= u[j];
        }
        take = outlen - done; if (take > 32) take = 32;
        for (k = 0; k < take; k++) out[done + k] = t[k];
        done += take; blk++;
    }
}

/* -- Salsa20/8 core + scryptBlockMix + scryptROMix (RFC 7914) ------------- */

static void salsa20_8(uint32_t out[16], const uint32_t in[16])
{
    uint32_t x[16];
    int i;
    for (i = 0; i < 16; i++) x[i] = in[i];
    for (i = 0; i < 8; i += 2) {
        /* column round */
        x[ 4] ^= ROTR32(x[ 0]+x[12], 32-7);  x[ 8] ^= ROTR32(x[ 4]+x[ 0], 32-9);
        x[12] ^= ROTR32(x[ 8]+x[ 4], 32-13); x[ 0] ^= ROTR32(x[12]+x[ 8], 32-18);
        x[ 9] ^= ROTR32(x[ 5]+x[ 1], 32-7);  x[13] ^= ROTR32(x[ 9]+x[ 5], 32-9);
        x[ 1] ^= ROTR32(x[13]+x[ 9], 32-13); x[ 5] ^= ROTR32(x[ 1]+x[13], 32-18);
        x[14] ^= ROTR32(x[10]+x[ 6], 32-7);  x[ 2] ^= ROTR32(x[14]+x[10], 32-9);
        x[ 6] ^= ROTR32(x[ 2]+x[14], 32-13); x[10] ^= ROTR32(x[ 6]+x[ 2], 32-18);
        x[ 3] ^= ROTR32(x[15]+x[11], 32-7);  x[ 7] ^= ROTR32(x[ 3]+x[15], 32-9);
        x[11] ^= ROTR32(x[ 7]+x[ 3], 32-13); x[15] ^= ROTR32(x[11]+x[ 7], 32-18);
        /* row round */
        x[ 1] ^= ROTR32(x[ 0]+x[ 3], 32-7);  x[ 2] ^= ROTR32(x[ 1]+x[ 0], 32-9);
        x[ 3] ^= ROTR32(x[ 2]+x[ 1], 32-13); x[ 0] ^= ROTR32(x[ 3]+x[ 2], 32-18);
        x[ 6] ^= ROTR32(x[ 5]+x[ 4], 32-7);  x[ 7] ^= ROTR32(x[ 6]+x[ 5], 32-9);
        x[ 4] ^= ROTR32(x[ 7]+x[ 6], 32-13); x[ 5] ^= ROTR32(x[ 4]+x[ 7], 32-18);
        x[11] ^= ROTR32(x[10]+x[ 9], 32-7);  x[ 8] ^= ROTR32(x[11]+x[10], 32-9);
        x[ 9] ^= ROTR32(x[ 8]+x[11], 32-13); x[10] ^= ROTR32(x[ 9]+x[ 8], 32-18);
        x[12] ^= ROTR32(x[15]+x[14], 32-7);  x[13] ^= ROTR32(x[12]+x[15], 32-9);
        x[14] ^= ROTR32(x[13]+x[12], 32-13); x[15] ^= ROTR32(x[14]+x[13], 32-18);
    }
    for (i = 0; i < 16; i++) out[i] = x[i] + in[i];
}

static void le32dec_block(uint32_t *dst, const unsigned char *src, size_t words)
{
    size_t i;
    for (i = 0; i < words; i++)
        dst[i] = (uint32_t)src[i*4] | ((uint32_t)src[i*4+1] << 8)
               | ((uint32_t)src[i*4+2] << 16) | ((uint32_t)src[i*4+3] << 24);
}

static void le32enc_block(unsigned char *dst, const uint32_t *src, size_t words)
{
    size_t i;
    for (i = 0; i < words; i++) {
        dst[i*4]   = (unsigned char)(src[i] & 0xff);
        dst[i*4+1] = (unsigned char)((src[i] >> 8) & 0xff);
        dst[i*4+2] = (unsigned char)((src[i] >> 16) & 0xff);
        dst[i*4+3] = (unsigned char)((src[i] >> 24) & 0xff);
    }
}

/* B is 2*r blocks of 64 bytes; Y is scratch of the same size. */
static void block_mix(uint32_t *B, uint32_t *Y, unsigned int r)
{
    uint32_t X[16];
    unsigned int i;
    memcpy(X, &B[(2*r - 1) * 16], 64);
    for (i = 0; i < 2*r; i++) {
        unsigned int j;
        for (j = 0; j < 16; j++) X[j] ^= B[i*16 + j];
        salsa20_8(X, X);
        memcpy(&Y[i*16], X, 64);
    }
    /* B' = (Y[0], Y[2], ..., Y[1], Y[3], ...) */
    for (i = 0; i < r; i++) {
        memcpy(&B[i*16], &Y[(i*2) * 16], 64);
        memcpy(&B[(i + r) * 16], &Y[(i*2 + 1) * 16], 64);
    }
}

static void romix(uint32_t *B, uint32_t *V, uint32_t *XY,
                  unsigned long N, unsigned int r)
{
    uint32_t *X = B, *Y = XY;
    unsigned long i;
    size_t words = 32 * r;              /* 128*r bytes */
    for (i = 0; i < N; i++) {
        memcpy(&V[i * words], X, words * 4);
        block_mix(X, Y, r);
    }
    for (i = 0; i < N; i++) {
        /* Integerify: the LAST 64-byte block, first word, little-endian. */
        unsigned long j = X[(2*r - 1) * 16] & (N - 1);
        size_t k;
        for (k = 0; k < words; k++) X[k] ^= V[j * words + k];
        block_mix(X, Y, r);
    }
}

int tsauth_scrypt(const unsigned char *pass, size_t passlen,
                  const unsigned char *salt, size_t saltlen,
                  unsigned long N, unsigned int r, unsigned int p,
                  unsigned char *out, size_t outlen)
{
    unsigned char *B = NULL;
    uint32_t *V = NULL, *XY = NULL, *Bw = NULL;
    size_t Blen, words;
    unsigned int i;

    /* N must be a power of two greater than 1: Integerify masks with N-1. */
    if (N < 2 || (N & (N - 1)) != 0) return -1;
    if (r == 0 || p == 0) return -1;
    /* Bound the allocation  -  these values reach us from stored credentials. */
    if (r > 32 || p > 32 || N > (1UL << 20)) return -1;

    words = 32 * r;
    Blen  = (size_t)p * 128 * r;
    B  = (unsigned char *)malloc(Blen);
    Bw = (uint32_t *)malloc(words * 4);
    XY = (uint32_t *)malloc(words * 4);
    V  = (uint32_t *)malloc((size_t)N * words * 4);
    if (!B || !Bw || !XY || !V) { free(B); free(Bw); free(XY); free(V); return -1; }

    pbkdf2_sha256(pass, passlen, salt, saltlen, 1, B, Blen);
    for (i = 0; i < p; i++) {
        le32dec_block(Bw, B + (size_t)i * 128 * r, words);
        romix(Bw, V, XY, N, r);
        le32enc_block(B + (size_t)i * 128 * r, Bw, words);
    }
    pbkdf2_sha256(pass, passlen, B, Blen, 1, out, outlen);

    memset(B, 0, Blen);
    free(B); free(Bw); free(XY); free(V);
    return 0;
}

/* -- stored-credential verification --------------------------------------- */

static int hexval(int ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

int tsauth_verify(const char *password, const char *stored)
{
    const char *colon;
    size_t saltlen, hexlen, i;
    unsigned char want[TSAUTH_DKLEN], got[TSAUTH_DKLEN];
    unsigned char diff = 0;

    if (!password || !stored) return 0;
    colon = strchr(stored, ':');
    if (!colon) return 0;

    saltlen = (size_t)(colon - stored);
    hexlen  = strlen(colon + 1);
    if (saltlen == 0 || hexlen != TSAUTH_DKLEN * 2) return 0;

    for (i = 0; i < TSAUTH_DKLEN; i++) {
        int hi = hexval((unsigned char)colon[1 + i*2]);
        int lo = hexval((unsigned char)colon[2 + i*2]);
        if (hi < 0 || lo < 0) return 0;
        want[i] = (unsigned char)((hi << 4) | lo);
    }

    /* !! The salt is the SALT STRING'S BYTES, not the decoded hex. */
    if (tsauth_scrypt((const unsigned char *)password, strlen(password),
                      (const unsigned char *)stored, saltlen,
                      TSAUTH_N, TSAUTH_R, TSAUTH_P, got, TSAUTH_DKLEN) != 0)
        return 0;

    for (i = 0; i < TSAUTH_DKLEN; i++) diff |= (unsigned char)(want[i] ^ got[i]);
    memset(got, 0, sizeof got);
    return diff == 0;
}

int tsauth_is_ts_hash(const char *stored)
{
    const char *colon;
    size_t i;
    if (!stored) return 0;
    colon = strchr(stored, ':');
    if (!colon || colon == stored) return 0;
    if (strlen(colon + 1) != TSAUTH_DKLEN * 2) return 0;
    for (i = 0; i < TSAUTH_DKLEN * 2; i++)
        if (hexval((unsigned char)colon[1 + i]) < 0) return 0;
    return 1;
}
