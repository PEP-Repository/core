#ifndef FE25519_H
#define FE25519_H

#include "crypto_ints.h"
#include "detect_uint128.h"

#ifdef HAVE_NATIVE_UINT128
# define FE25519_RADIX51
#endif

typedef struct
{
#ifdef FE25519_RADIX51
  crypto_uint64 v[5];
#else
  crypto_int32 v[10];
#endif
}
fe25519;



extern const fe25519 fe25519_zero;
extern const fe25519 fe25519_one;
extern const fe25519 fe25519_two;

extern const fe25519 fe25519_sqrtm1;
extern const fe25519 fe25519_msqrtm1;
extern const fe25519 fe25519_m1;

//void fe25519_freeze(fe25519 *r);

void fe25519_unpack(fe25519 *r, const unsigned char x[32]);

void fe25519_pack(unsigned char r[32], const fe25519 *x);

int fe25519_iszero(const fe25519 *x);

int fe25519_isone(const fe25519 *x);

int fe25519_isnegative(const fe25519 *x);

int fe25519_iseq(const fe25519 *x, const fe25519 *y);

int fe25519_iseq_vartime(const fe25519 *x, const fe25519 *y);

void fe25519_cmov(fe25519 *r, const fe25519 *x, unsigned char b);

void fe25519_setone(fe25519 *r);

void fe25519_setzero(fe25519 *r);

void fe25519_neg(fe25519 *r, const fe25519 *x);

unsigned char fe25519_getparity(const fe25519 *x);

void fe25519_add(fe25519 *r, const fe25519 *x, const fe25519 *y);

void fe25519_double(fe25519 *r, const fe25519 *x);
void fe25519_triple(fe25519 *r, const fe25519 *x);

void fe25519_sub(fe25519 *r, const fe25519 *x, const fe25519 *y);

void fe25519_mul(fe25519 *r, const fe25519 *x, const fe25519 *y);

void fe25519_square(fe25519 *r, const fe25519 *x);
void fe25519_square_double(fe25519 *h,const fe25519 *f);

void fe25519_invert(fe25519 *r, const fe25519 *x);

void fe25519_pow2523(fe25519 *r, const fe25519 *x);

void fe25519_invsqrt(fe25519 *r, const fe25519 *x);

// Sets r to 1/sqrt(x) or 1/sqrt(i*x).  Returns whether x was a square.
int fe25519_invsqrti(fe25519 *r, const fe25519 *x);

void fe25519_sqrt(fe25519 *r, const fe25519 *x);
//
// Sets r to sqrt(x) or sqrt(i * x).  Returns 1 if x is a square.
int fe25519_sqrti(fe25519 *r, const fe25519 *x);

void fe25519_abs(fe25519 *x, const fe25519 *y);

void fe25519_set_reduced(fe25519* r, const fe25519* h);

//void fe25519_print(const fe25519 *x);

#endif
