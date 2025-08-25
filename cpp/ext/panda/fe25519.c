// Based on Ed25519/ref10 in SUPERCOP https://bench.cr.yp.to/supercop.html

#include "crypto_ints.h"
#include "fe25519.h"

static crypto_uint32 equal32(crypto_uint32 a,crypto_uint32 b) /* 16-bit inputs */
{
  crypto_uint32 x = a ^ b; /* 0: yes; 1..65535: no */
  x -= 1; /* 4294967295: yes; 0..65534: no */
  x >>= 31; /* 1: yes; 0: no */
  return x;
}

#ifdef FE25519_RADIX51
# include "fe25519-51.c"
#else
# include "fe25519-25.5.c"
#endif


/*
 * return 1 if f == 0
 * return 0 if f != 0
 * 
 * Preconditions:
 *   |f| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
 */
static const unsigned char zero[32];
int fe25519_iszero(const fe25519 *f)
{
  int i,r=0;
  unsigned char s[32];
  fe25519_pack(s,f);
  for(i=0;i<32;i++)
    r |= (1-equal32(zero[i],s[i]));
  return 1-r;
}

int fe25519_isone(const fe25519 *x) 
{
  return fe25519_iseq(x, &fe25519_one);  
}  

/*
 * return 1 if f is in {1,3,5,...,q-2}
 * return 0 if f is in {0,2,4,...,q-1}
 * 
 * Preconditions:
 *   |f| bounded by 1.1*2^26,1.1*2^25,1.1*2^26,1.1*2^25,etc.
 */

int fe25519_isnegative(const fe25519 *f)
{
  unsigned char s[32];
  fe25519_pack(s,f);
  return s[0] & 1;
}

int fe25519_iseq(const fe25519 *x, const fe25519 *y)
{
  fe25519 t;
  fe25519_sub(&t, x, y);
  return fe25519_iszero(&t);
}

int fe25519_iseq_vartime(const fe25519 *x, const fe25519 *y) {
  return fe25519_iseq(x, y);
}  


unsigned char fe25519_getparity(const fe25519 *x) {
  return fe25519_isnegative(x);  
}  



void fe25519_double(fe25519 *r, const fe25519 *x) {
  fe25519_add(r, x, x);  
}

void fe25519_triple(fe25519 *r, const fe25519 *x) {
  fe25519_add(r, x, x);
  fe25519_add(r, r, x);
}


void fe25519_invert(fe25519 *out,const fe25519 *z)
{
  fe25519 t0;
  fe25519 t1;
  fe25519 t2;
  fe25519 t3;
  int i;
  
  /* qhasm: fe z1 */
  
  /* qhasm: fe z2 */
  
  /* qhasm: fe z8 */
  
  /* qhasm: fe z9 */
  
  /* qhasm: fe z11 */
  
  /* qhasm: fe z22 */
  
  /* qhasm: fe z_5_0 */
  
  /* qhasm: fe z_10_5 */
  
  /* qhasm: fe z_10_0 */
  
  /* qhasm: fe z_20_10 */
  
  /* qhasm: fe z_20_0 */
  
  /* qhasm: fe z_40_20 */
  
  /* qhasm: fe z_40_0 */
  
  /* qhasm: fe z_50_10 */
  
  /* qhasm: fe z_50_0 */
  
  /* qhasm: fe z_100_50 */
  
  /* qhasm: fe z_100_0 */
  
  /* qhasm: fe z_200_100 */
  
  /* qhasm: fe z_200_0 */
  
  /* qhasm: fe z_250_50 */
  
  /* qhasm: fe z_250_0 */
  
  /* qhasm: fe z_255_5 */
  
  /* qhasm: fe z_255_21 */
  
  /* qhasm: enter pow225521 */
  
  /* qhasm: z2 = z1^2^1 */
  /* asm 1: fe25519_square(>z2=fe#1,<z1=fe#11); for (i = 1;i < 1;++i) fe25519_square(>z2=fe#1,>z2=fe#1); */
  /* asm 2: fe25519_square(>z2=&t0,<z1=z); for (i = 1;i < 1;++i) fe25519_square(>z2=&t0,>z2=&t0); */
  fe25519_square(&t0,z); for (i = 1;i < 1;++i) fe25519_square(&t0,&t0);
  
  /* qhasm: z8 = z2^2^2 */
  /* asm 1: fe25519_square(>z8=fe#2,<z2=fe#1); for (i = 1;i < 2;++i) fe25519_square(>z8=fe#2,>z8=fe#2); */
  /* asm 2: fe25519_square(>z8=&t1,<z2=&t0); for (i = 1;i < 2;++i) fe25519_square(>z8=&t1,>z8=&t1); */
  fe25519_square(&t1,&t0); for (i = 1;i < 2;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z9 = z1*z8 */
  /* asm 1: fe25519_mul(>z9=fe#2,<z1=fe#11,<z8=fe#2); */
  /* asm 2: fe25519_mul(>z9=&t1,<z1=z,<z8=&t1); */
  fe25519_mul(&t1,z,&t1);
  
  /* qhasm: z11 = z2*z9 */
  /* asm 1: fe25519_mul(>z11=fe#1,<z2=fe#1,<z9=fe#2); */
  /* asm 2: fe25519_mul(>z11=&t0,<z2=&t0,<z9=&t1); */
  fe25519_mul(&t0,&t0,&t1);
  
  /* qhasm: z22 = z11^2^1 */
  /* asm 1: fe25519_square(>z22=fe#3,<z11=fe#1); for (i = 1;i < 1;++i) fe25519_square(>z22=fe#3,>z22=fe#3); */
  /* asm 2: fe25519_square(>z22=&t2,<z11=&t0); for (i = 1;i < 1;++i) fe25519_square(>z22=&t2,>z22=&t2); */
  fe25519_square(&t2,&t0); for (i = 1;i < 1;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_5_0 = z9*z22 */
  /* asm 1: fe25519_mul(>z_5_0=fe#2,<z9=fe#2,<z22=fe#3); */
  /* asm 2: fe25519_mul(>z_5_0=&t1,<z9=&t1,<z22=&t2); */
  fe25519_mul(&t1,&t1,&t2);
  
  /* qhasm: z_10_5 = z_5_0^2^5 */
  /* asm 1: fe25519_square(>z_10_5=fe#3,<z_5_0=fe#2); for (i = 1;i < 5;++i) fe25519_square(>z_10_5=fe#3,>z_10_5=fe#3); */
  /* asm 2: fe25519_square(>z_10_5=&t2,<z_5_0=&t1); for (i = 1;i < 5;++i) fe25519_square(>z_10_5=&t2,>z_10_5=&t2); */
  fe25519_square(&t2,&t1); for (i = 1;i < 5;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_10_0 = z_10_5*z_5_0 */
  /* asm 1: fe25519_mul(>z_10_0=fe#2,<z_10_5=fe#3,<z_5_0=fe#2); */
  /* asm 2: fe25519_mul(>z_10_0=&t1,<z_10_5=&t2,<z_5_0=&t1); */
  fe25519_mul(&t1,&t2,&t1);
  
  /* qhasm: z_20_10 = z_10_0^2^10 */
  /* asm 1: fe25519_square(>z_20_10=fe#3,<z_10_0=fe#2); for (i = 1;i < 10;++i) fe25519_square(>z_20_10=fe#3,>z_20_10=fe#3); */
  /* asm 2: fe25519_square(>z_20_10=&t2,<z_10_0=&t1); for (i = 1;i < 10;++i) fe25519_square(>z_20_10=&t2,>z_20_10=&t2); */
  fe25519_square(&t2,&t1); for (i = 1;i < 10;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_20_0 = z_20_10*z_10_0 */
  /* asm 1: fe25519_mul(>z_20_0=fe#3,<z_20_10=fe#3,<z_10_0=fe#2); */
  /* asm 2: fe25519_mul(>z_20_0=&t2,<z_20_10=&t2,<z_10_0=&t1); */
  fe25519_mul(&t2,&t2,&t1);
  
  /* qhasm: z_40_20 = z_20_0^2^20 */
  /* asm 1: fe25519_square(>z_40_20=fe#4,<z_20_0=fe#3); for (i = 1;i < 20;++i) fe25519_square(>z_40_20=fe#4,>z_40_20=fe#4); */
  /* asm 2: fe25519_square(>z_40_20=&t3,<z_20_0=&t2); for (i = 1;i < 20;++i) fe25519_square(>z_40_20=&t3,>z_40_20=&t3); */
  fe25519_square(&t3,&t2); for (i = 1;i < 20;++i) fe25519_square(&t3,&t3);
  
  /* qhasm: z_40_0 = z_40_20*z_20_0 */
  /* asm 1: fe25519_mul(>z_40_0=fe#3,<z_40_20=fe#4,<z_20_0=fe#3); */
  /* asm 2: fe25519_mul(>z_40_0=&t2,<z_40_20=&t3,<z_20_0=&t2); */
  fe25519_mul(&t2,&t3,&t2);
  
  /* qhasm: z_50_10 = z_40_0^2^10 */
  /* asm 1: fe25519_square(>z_50_10=fe#3,<z_40_0=fe#3); for (i = 1;i < 10;++i) fe25519_square(>z_50_10=fe#3,>z_50_10=fe#3); */
  /* asm 2: fe25519_square(>z_50_10=&t2,<z_40_0=&t2); for (i = 1;i < 10;++i) fe25519_square(>z_50_10=&t2,>z_50_10=&t2); */
  fe25519_square(&t2,&t2); for (i = 1;i < 10;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_50_0 = z_50_10*z_10_0 */
  /* asm 1: fe25519_mul(>z_50_0=fe#2,<z_50_10=fe#3,<z_10_0=fe#2); */
  /* asm 2: fe25519_mul(>z_50_0=&t1,<z_50_10=&t2,<z_10_0=&t1); */
  fe25519_mul(&t1,&t2,&t1);
  
  /* qhasm: z_100_50 = z_50_0^2^50 */
  /* asm 1: fe25519_square(>z_100_50=fe#3,<z_50_0=fe#2); for (i = 1;i < 50;++i) fe25519_square(>z_100_50=fe#3,>z_100_50=fe#3); */
  /* asm 2: fe25519_square(>z_100_50=&t2,<z_50_0=&t1); for (i = 1;i < 50;++i) fe25519_square(>z_100_50=&t2,>z_100_50=&t2); */
  fe25519_square(&t2,&t1); for (i = 1;i < 50;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_100_0 = z_100_50*z_50_0 */
  /* asm 1: fe25519_mul(>z_100_0=fe#3,<z_100_50=fe#3,<z_50_0=fe#2); */
  /* asm 2: fe25519_mul(>z_100_0=&t2,<z_100_50=&t2,<z_50_0=&t1); */
  fe25519_mul(&t2,&t2,&t1);
  
  /* qhasm: z_200_100 = z_100_0^2^100 */
  /* asm 1: fe25519_square(>z_200_100=fe#4,<z_100_0=fe#3); for (i = 1;i < 100;++i) fe25519_square(>z_200_100=fe#4,>z_200_100=fe#4); */
  /* asm 2: fe25519_square(>z_200_100=&t3,<z_100_0=&t2); for (i = 1;i < 100;++i) fe25519_square(>z_200_100=&t3,>z_200_100=&t3); */
  fe25519_square(&t3,&t2); for (i = 1;i < 100;++i) fe25519_square(&t3,&t3);
  
  /* qhasm: z_200_0 = z_200_100*z_100_0 */
  /* asm 1: fe25519_mul(>z_200_0=fe#3,<z_200_100=fe#4,<z_100_0=fe#3); */
  /* asm 2: fe25519_mul(>z_200_0=&t2,<z_200_100=&t3,<z_100_0=&t2); */
  fe25519_mul(&t2,&t3,&t2);
  
  /* qhasm: z_250_50 = z_200_0^2^50 */
  /* asm 1: fe25519_square(>z_250_50=fe#3,<z_200_0=fe#3); for (i = 1;i < 50;++i) fe25519_square(>z_250_50=fe#3,>z_250_50=fe#3); */
  /* asm 2: fe25519_square(>z_250_50=&t2,<z_200_0=&t2); for (i = 1;i < 50;++i) fe25519_square(>z_250_50=&t2,>z_250_50=&t2); */
  fe25519_square(&t2,&t2); for (i = 1;i < 50;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_250_0 = z_250_50*z_50_0 */
  /* asm 1: fe25519_mul(>z_250_0=fe#2,<z_250_50=fe#3,<z_50_0=fe#2); */
  /* asm 2: fe25519_mul(>z_250_0=&t1,<z_250_50=&t2,<z_50_0=&t1); */
  fe25519_mul(&t1,&t2,&t1);
  
  /* qhasm: z_255_5 = z_250_0^2^5 */
  /* asm 1: fe25519_square(>z_255_5=fe#2,<z_250_0=fe#2); for (i = 1;i < 5;++i) fe25519_square(>z_255_5=fe#2,>z_255_5=fe#2); */
  /* asm 2: fe25519_square(>z_255_5=&t1,<z_250_0=&t1); for (i = 1;i < 5;++i) fe25519_square(>z_255_5=&t1,>z_255_5=&t1); */
  fe25519_square(&t1,&t1); for (i = 1;i < 5;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z_255_21 = z_255_5*z11 */
  /* asm 1: fe25519_mul(>z_255_21=fe#12,<z_255_5=fe#2,<z11=fe#1); */
  /* asm 2: fe25519_mul(>z_255_21=out,<z_255_5=&t1,<z11=&t0); */
  fe25519_mul(out,&t1,&t0);
  
  /* qhasm: return */
  
  return;
}


void fe25519_pow2523(fe25519 *out,const fe25519 *z)
{
  fe25519 t0;
  fe25519 t1;
  fe25519 t2;
  int i;
  
  /* qhasm: fe z1 */
  
  /* qhasm: fe z2 */
  
  /* qhasm: fe z8 */
  
  /* qhasm: fe z9 */
  
  /* qhasm: fe z11 */
  
  /* qhasm: fe z22 */
  
  /* qhasm: fe z_5_0 */
  
  /* qhasm: fe z_10_5 */
  
  /* qhasm: fe z_10_0 */
  
  /* qhasm: fe z_20_10 */
  
  /* qhasm: fe z_20_0 */
  
  /* qhasm: fe z_40_20 */
  
  /* qhasm: fe z_40_0 */
  
  /* qhasm: fe z_50_10 */
  
  /* qhasm: fe z_50_0 */
  
  /* qhasm: fe z_100_50 */
  
  /* qhasm: fe z_100_0 */
  
  /* qhasm: fe z_200_100 */
  
  /* qhasm: fe z_200_0 */
  
  /* qhasm: fe z_250_50 */
  
  /* qhasm: fe z_250_0 */
  
  /* qhasm: fe z_252_2 */
  
  /* qhasm: fe z_252_3 */
  
  /* qhasm: enter pow22523 */
  
  /* qhasm: z2 = z1^2^1 */
  /* asm 1: fe25519_square(>z2=fe#1,<z1=fe#11); for (i = 1;i < 1;++i) fe25519_square(>z2=fe#1,>z2=fe#1); */
  /* asm 2: fe25519_square(>z2=&t0,<z1=z); for (i = 1;i < 1;++i) fe25519_square(>z2=&t0,>z2=&t0); */
  fe25519_square(&t0,z); for (i = 1;i < 1;++i) fe25519_square(&t0,&t0);
  
  /* qhasm: z8 = z2^2^2 */
  /* asm 1: fe25519_square(>z8=fe#2,<z2=fe#1); for (i = 1;i < 2;++i) fe25519_square(>z8=fe#2,>z8=fe#2); */
  /* asm 2: fe25519_square(>z8=&t1,<z2=&t0); for (i = 1;i < 2;++i) fe25519_square(>z8=&t1,>z8=&t1); */
  fe25519_square(&t1,&t0); for (i = 1;i < 2;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z9 = z1*z8 */
  /* asm 1: fe25519_mul(>z9=fe#2,<z1=fe#11,<z8=fe#2); */
  /* asm 2: fe25519_mul(>z9=&t1,<z1=z,<z8=&t1); */
  fe25519_mul(&t1,z,&t1);
  
  /* qhasm: z11 = z2*z9 */
  /* asm 1: fe25519_mul(>z11=fe#1,<z2=fe#1,<z9=fe#2); */
  /* asm 2: fe25519_mul(>z11=&t0,<z2=&t0,<z9=&t1); */
  fe25519_mul(&t0,&t0,&t1);
  
  /* qhasm: z22 = z11^2^1 */
  /* asm 1: fe25519_square(>z22=fe#1,<z11=fe#1); for (i = 1;i < 1;++i) fe25519_square(>z22=fe#1,>z22=fe#1); */
  /* asm 2: fe25519_square(>z22=&t0,<z11=&t0); for (i = 1;i < 1;++i) fe25519_square(>z22=&t0,>z22=&t0); */
  fe25519_square(&t0,&t0); for (i = 1;i < 1;++i) fe25519_square(&t0,&t0);
  
  /* qhasm: z_5_0 = z9*z22 */
  /* asm 1: fe25519_mul(>z_5_0=fe#1,<z9=fe#2,<z22=fe#1); */
  /* asm 2: fe25519_mul(>z_5_0=&t0,<z9=&t1,<z22=&t0); */
  fe25519_mul(&t0,&t1,&t0);
  
  /* qhasm: z_10_5 = z_5_0^2^5 */
  /* asm 1: fe25519_square(>z_10_5=fe#2,<z_5_0=fe#1); for (i = 1;i < 5;++i) fe25519_square(>z_10_5=fe#2,>z_10_5=fe#2); */
  /* asm 2: fe25519_square(>z_10_5=&t1,<z_5_0=&t0); for (i = 1;i < 5;++i) fe25519_square(>z_10_5=&t1,>z_10_5=&t1); */
  fe25519_square(&t1,&t0); for (i = 1;i < 5;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z_10_0 = z_10_5*z_5_0 */
  /* asm 1: fe25519_mul(>z_10_0=fe#1,<z_10_5=fe#2,<z_5_0=fe#1); */
  /* asm 2: fe25519_mul(>z_10_0=&t0,<z_10_5=&t1,<z_5_0=&t0); */
  fe25519_mul(&t0,&t1,&t0);
  
  /* qhasm: z_20_10 = z_10_0^2^10 */
  /* asm 1: fe25519_square(>z_20_10=fe#2,<z_10_0=fe#1); for (i = 1;i < 10;++i) fe25519_square(>z_20_10=fe#2,>z_20_10=fe#2); */
  /* asm 2: fe25519_square(>z_20_10=&t1,<z_10_0=&t0); for (i = 1;i < 10;++i) fe25519_square(>z_20_10=&t1,>z_20_10=&t1); */
  fe25519_square(&t1,&t0); for (i = 1;i < 10;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z_20_0 = z_20_10*z_10_0 */
  /* asm 1: fe25519_mul(>z_20_0=fe#2,<z_20_10=fe#2,<z_10_0=fe#1); */
  /* asm 2: fe25519_mul(>z_20_0=&t1,<z_20_10=&t1,<z_10_0=&t0); */
  fe25519_mul(&t1,&t1,&t0);
  
  /* qhasm: z_40_20 = z_20_0^2^20 */
  /* asm 1: fe25519_square(>z_40_20=fe#3,<z_20_0=fe#2); for (i = 1;i < 20;++i) fe25519_square(>z_40_20=fe#3,>z_40_20=fe#3); */
  /* asm 2: fe25519_square(>z_40_20=&t2,<z_20_0=&t1); for (i = 1;i < 20;++i) fe25519_square(>z_40_20=&t2,>z_40_20=&t2); */
  fe25519_square(&t2,&t1); for (i = 1;i < 20;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_40_0 = z_40_20*z_20_0 */
  /* asm 1: fe25519_mul(>z_40_0=fe#2,<z_40_20=fe#3,<z_20_0=fe#2); */
  /* asm 2: fe25519_mul(>z_40_0=&t1,<z_40_20=&t2,<z_20_0=&t1); */
  fe25519_mul(&t1,&t2,&t1);
  
  /* qhasm: z_50_10 = z_40_0^2^10 */
  /* asm 1: fe25519_square(>z_50_10=fe#2,<z_40_0=fe#2); for (i = 1;i < 10;++i) fe25519_square(>z_50_10=fe#2,>z_50_10=fe#2); */
  /* asm 2: fe25519_square(>z_50_10=&t1,<z_40_0=&t1); for (i = 1;i < 10;++i) fe25519_square(>z_50_10=&t1,>z_50_10=&t1); */
  fe25519_square(&t1,&t1); for (i = 1;i < 10;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z_50_0 = z_50_10*z_10_0 */
  /* asm 1: fe25519_mul(>z_50_0=fe#1,<z_50_10=fe#2,<z_10_0=fe#1); */
  /* asm 2: fe25519_mul(>z_50_0=&t0,<z_50_10=&t1,<z_10_0=&t0); */
  fe25519_mul(&t0,&t1,&t0);
  
  /* qhasm: z_100_50 = z_50_0^2^50 */
  /* asm 1: fe25519_square(>z_100_50=fe#2,<z_50_0=fe#1); for (i = 1;i < 50;++i) fe25519_square(>z_100_50=fe#2,>z_100_50=fe#2); */
  /* asm 2: fe25519_square(>z_100_50=&t1,<z_50_0=&t0); for (i = 1;i < 50;++i) fe25519_square(>z_100_50=&t1,>z_100_50=&t1); */
  fe25519_square(&t1,&t0); for (i = 1;i < 50;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z_100_0 = z_100_50*z_50_0 */
  /* asm 1: fe25519_mul(>z_100_0=fe#2,<z_100_50=fe#2,<z_50_0=fe#1); */
  /* asm 2: fe25519_mul(>z_100_0=&t1,<z_100_50=&t1,<z_50_0=&t0); */
  fe25519_mul(&t1,&t1,&t0);
  
  /* qhasm: z_200_100 = z_100_0^2^100 */
  /* asm 1: fe25519_square(>z_200_100=fe#3,<z_100_0=fe#2); for (i = 1;i < 100;++i) fe25519_square(>z_200_100=fe#3,>z_200_100=fe#3); */
  /* asm 2: fe25519_square(>z_200_100=&t2,<z_100_0=&t1); for (i = 1;i < 100;++i) fe25519_square(>z_200_100=&t2,>z_200_100=&t2); */
  fe25519_square(&t2,&t1); for (i = 1;i < 100;++i) fe25519_square(&t2,&t2);
  
  /* qhasm: z_200_0 = z_200_100*z_100_0 */
  /* asm 1: fe25519_mul(>z_200_0=fe#2,<z_200_100=fe#3,<z_100_0=fe#2); */
  /* asm 2: fe25519_mul(>z_200_0=&t1,<z_200_100=&t2,<z_100_0=&t1); */
  fe25519_mul(&t1,&t2,&t1);
  
  /* qhasm: z_250_50 = z_200_0^2^50 */
  /* asm 1: fe25519_square(>z_250_50=fe#2,<z_200_0=fe#2); for (i = 1;i < 50;++i) fe25519_square(>z_250_50=fe#2,>z_250_50=fe#2); */
  /* asm 2: fe25519_square(>z_250_50=&t1,<z_200_0=&t1); for (i = 1;i < 50;++i) fe25519_square(>z_250_50=&t1,>z_250_50=&t1); */
  fe25519_square(&t1,&t1); for (i = 1;i < 50;++i) fe25519_square(&t1,&t1);
  
  /* qhasm: z_250_0 = z_250_50*z_50_0 */
  /* asm 1: fe25519_mul(>z_250_0=fe#1,<z_250_50=fe#2,<z_50_0=fe#1); */
  /* asm 2: fe25519_mul(>z_250_0=&t0,<z_250_50=&t1,<z_50_0=&t0); */
  fe25519_mul(&t0,&t1,&t0);
  
  /* qhasm: z_252_2 = z_250_0^2^2 */
  /* asm 1: fe25519_square(>z_252_2=fe#1,<z_250_0=fe#1); for (i = 1;i < 2;++i) fe25519_square(>z_252_2=fe#1,>z_252_2=fe#1); */
  /* asm 2: fe25519_square(>z_252_2=&t0,<z_250_0=&t0); for (i = 1;i < 2;++i) fe25519_square(>z_252_2=&t0,>z_252_2=&t0); */
  fe25519_square(&t0,&t0); for (i = 1;i < 2;++i) fe25519_square(&t0,&t0);
  
  /* qhasm: z_252_3 = z_252_2*z1 */
  /* asm 1: fe25519_mul(>z_252_3=fe#12,<z_252_2=fe#1,<z1=fe#11); */
  /* asm 2: fe25519_mul(>z_252_3=out,<z_252_2=&t0,<z1=z); */
  fe25519_mul(out,&t0,z);
  
  /* qhasm: return */
  
  
  return;
}

void fe25519_sqrt(fe25519 *r, const fe25519 *x)
{
  fe25519 t;
  fe25519_invsqrt(&t, x);
  fe25519_mul(r, &t, x);
}

// Sets r to sqrt(x) or sqrt(i * x).  Returns 1 if x is a square.
int fe25519_sqrti(fe25519 *r, const fe25519 *x)
{
  int b;
  fe25519 t, corr;
  b = fe25519_invsqrti(&t, x);
  fe25519_setone(&corr);
  fe25519_cmov(&corr, &fe25519_sqrtm1, 1 - b);
  fe25519_mul(&t, &t, &corr);
  fe25519_mul(r, &t, x);
  return b;
}

// Sets r to 1/sqrt(x) or 1/sqrt(i*x).  Returns whether x was a square.
int fe25519_invsqrti(fe25519 *r, const fe25519 *x)
{
  int inCaseA, inCaseB, inCaseD;
  fe25519 den2, den3, den4, den6, chk, t, corr;
  fe25519_square(&den2, x);
  fe25519_mul(&den3, &den2, x);
  
  fe25519_square(&den4, &den2);
  fe25519_mul(&den6, &den2, &den4);
  fe25519_mul(&t, &den6, x); // r is now x^7
  
  fe25519_pow2523(&t, &t);
  fe25519_mul(&t, &t, &den3);
   
  // case       A           B            C             D
  // ---------------------------------------------------------------
  // t          1/sqrt(x)   -i/sqrt(x)   1/sqrt(i*x)   -i/sqrt(i*x)
  // chk        1           -1           -i            i
  // corr       1           i            1             i
  // ret        1           1            0             0
  fe25519_square(&chk, &t);
  fe25519_mul(&chk, &chk, x);

  inCaseA = fe25519_isone(&chk);
  inCaseD = fe25519_iseq(&chk, &fe25519_sqrtm1);
  fe25519_neg(&chk, &chk);
  inCaseB = fe25519_isone(&chk);

  fe25519_setone(&corr);
  fe25519_cmov(&corr, &fe25519_sqrtm1, inCaseB + inCaseD);
  fe25519_mul(&t, &t, &corr);
  
  *r = t;

  return inCaseA + inCaseB;
}

void fe25519_invsqrt(fe25519 *r, const fe25519 *x)
{
  fe25519 den2, den3, den4, den6, chk, t, t2;
  int b;

  fe25519_square(&den2, x);
  fe25519_mul(&den3, &den2, x);
  
  fe25519_square(&den4, &den2);
  fe25519_mul(&den6, &den2, &den4);
  fe25519_mul(&t, &den6, x); // r is now x^7
  
  fe25519_pow2523(&t, &t);
  fe25519_mul(&t, &t, &den3);
  
  fe25519_square(&chk, &t);
  fe25519_mul(&chk, &chk, x);
  
  fe25519_mul(&t2, &t, &fe25519_sqrtm1);
  b = 1 - fe25519_isone(&chk);
  fe25519_cmov(&t, &t2, b);
  
  *r = t;
}

// Return x if x is positive, else return -x
void fe25519_abs(fe25519* x, const fe25519* y)
{
    fe25519 negY;
    *x = *y;
    fe25519_neg(&negY, y);
    fe25519_cmov(x, &negY, fe25519_isnegative(x));
}

/*
void fe25519_print(const fe25519 *x)
{
  printf("(");
  printf("%d + ",x->v[0]);
  printf("%d*2^26 + ",x->v[1]);
  printf("%d*2^51 + ",x->v[2]);
  printf("%d*2^77 + ",x->v[3]);
  printf("%d*2^102 + ",x->v[4]);
  printf("%d*2^128 + ",x->v[5]);
  printf("%d*2^153 + ",x->v[6]);
  printf("%d*2^179 + ",x->v[7]);
  printf("%d*2^204 + ",x->v[8]);
  printf("%d*2^230 + (2^255-19)) % (2^255-19)\n",x->v[9]);
}
*/

