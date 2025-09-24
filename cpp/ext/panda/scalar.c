#include "scalar.h"
#include "crypto_ints.h"

const group_scalar group_scalar_zero  = {{0}};
const group_scalar group_scalar_one   = {{1}};

static const crypto_uint32 m[32] = {0xED, 0xD3, 0xF5, 0x5C, 0x1A, 0x63, 0x12, 0x58, 0xD6, 0x9C, 0xF7, 0xA2, 0xDE, 0xF9, 0xDE, 0x14,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};

static crypto_int64 load_3_32(const uint32_t *in)
{
  crypto_int64 result;
  result = (crypto_int64) in[0];
  result |= ((crypto_int64) in[1]) << 8;
  result |= ((crypto_int64) in[2]) << 16;
  return result;
}

static crypto_int64 load_4_32(const uint32_t *in)
{
  crypto_int64 result;
  result = (crypto_int64) in[0];
  result |= ((crypto_int64) in[1]) << 8;
  result |= ((crypto_int64) in[2]) << 16;
  result |= ((crypto_int64) in[3]) << 24;
  return result;
}

static uint64_t load_8u(const uint32_t *in)
{
  return (((uint64_t)in[0]) |
          (((uint64_t)in[1]) << 8 ) |
          (((uint64_t)in[2]) << 16) |
          (((uint64_t)in[3]) << 24) |
          (((uint64_t)in[4]) << 32) |
          (((uint64_t)in[5]) << 40) |
          (((uint64_t)in[6]) << 48) |
          (((uint64_t)in[7]) << 56));
}

static crypto_uint32 lt(crypto_uint32 a,crypto_uint32 b) /* 16-bit inputs */
{
  unsigned int x = a;
  x -= (unsigned int) b; /* 0..65535: no; 4294901761..4294967295: yes */
  x >>= 31; /* 0: no; 1: yes */
  return x;
}

/* Reduce coefficients of r before calling reduce_add_sub */
static void reduce_add_sub(group_scalar *r)
{
  crypto_uint32 pb = 0;
  crypto_uint32 b;
  crypto_uint32 mask;
  int i;
  unsigned char t[32];

  for(i=0;i<32;i++)
  {
    pb += m[i];
    b = lt(r->v[i],pb);
    t[i] = (unsigned char)(r->v[i]-pb+(b<<8));
    pb = b;
  }
  mask = b - 1;
  for(i=0;i<32;i++)
    r->v[i] ^= mask & (r->v[i] ^ t[i]);
}

int  group_scalar_unpack(group_scalar *r, const unsigned char x[GROUP_SCALAR_PACKEDBYTES])
{
  int i;
  for(i=0;i<32;i++)
    r->v[i] = x[i];
  r->v[31] &= 0x1f;
  reduce_add_sub(r);
  return 0;
}

void group_scalar_pack(unsigned char r[GROUP_SCALAR_PACKEDBYTES], const group_scalar *x)
{
  int i;
  for(i=0;i<32;i++)
    r[i] = (unsigned char)x->v[i];
}

void group_scalar_setzero(group_scalar *r)
{
  int i;
  for(i=0;i<32;i++)
    r->v[i] = 0;
}

void group_scalar_setone(group_scalar *r)
{
  int i;
  r->v[0] = 1;
  for(i=1;i<32;i++)
    r->v[i] = 0;
}

void group_scalar_add(group_scalar *r, const group_scalar *x, const group_scalar *y)
{
  int i, carry;
  for(i=0;i<32;i++) r->v[i] = x->v[i] + y->v[i];
  for(i=0;i<31;i++)
  {
    carry = r->v[i] >> 8;
    r->v[i+1] += (uint32_t)carry;
    r->v[i] &= 0xff;
  }
  reduce_add_sub(r);
}

void group_scalar_sub(group_scalar *r, const group_scalar *x, const group_scalar *y)
{
  crypto_uint32 b = 0;
  crypto_uint32 t;
  int i;
  group_scalar d;

  for(i=0;i<32;i++)
  {
    t = m[i] - y->v[i] - b;
    d.v[i] = t & 255;
    b = (t >> 8) & 1;
  }
  group_scalar_add(r,x,&d);
}

void group_scalar_negate(group_scalar *r, const group_scalar *x)
{
  group_scalar t;
  group_scalar_setzero(&t);
  group_scalar_sub(r,&t,x);
}

void group_scalar_mul(group_scalar *r, const group_scalar *x, const group_scalar *y)
{
  crypto_int64 a0 = 0x1fffff & load_3_32(x->v);
  crypto_int64 a1 = 0x1fffff & (load_4_32(x->v+2) >> 5);
  crypto_int64 a2 = 0x1fffff & (load_3_32(x->v+5) >> 2);
  crypto_int64 a3 = 0x1fffff & (load_4_32(x->v+7) >> 7);
  crypto_int64 a4 = 0x1fffff & (load_4_32(x->v+10)>> 4);
  crypto_int64 a5 = 0x1fffff & (load_3_32(x->v+13)>> 1);
  crypto_int64 a6 = 0x1fffff & (load_4_32(x->v+15)>> 6);
  crypto_int64 a7 = 0x1fffff & (load_3_32(x->v+18)>> 3);
  crypto_int64 a8 = 0x1fffff & load_3_32(x->v+21);
  crypto_int64 a9 = 0x1fffff & (load_4_32(x->v+23)>> 5);
  crypto_int64 a10 =0x1fffff & (load_3_32(x->v+26)>> 2);
  crypto_int64 a11 = load_4_32(x->v+28) >> 7;

  crypto_int64 b0 = 0x1fffff & load_3_32(y->v);
  crypto_int64 b1 = 0x1fffff & (load_4_32(y->v+2) >> 5);
  crypto_int64 b2 = 0x1fffff & (load_3_32(y->v+5) >> 2);
  crypto_int64 b3 = 0x1fffff & (load_4_32(y->v+7) >> 7);
  crypto_int64 b4 = 0x1fffff & (load_4_32(y->v+10)>> 4);
  crypto_int64 b5 = 0x1fffff & (load_3_32(y->v+13)>> 1);
  crypto_int64 b6 = 0x1fffff & (load_4_32(y->v+15)>> 6);
  crypto_int64 b7 = 0x1fffff & (load_3_32(y->v+18)>> 3);
  crypto_int64 b8 = 0x1fffff & load_3_32(y->v+21);
  crypto_int64 b9 = 0x1fffff & (load_4_32(y->v+23)>> 5);
  crypto_int64 b10 =0x1fffff & (load_3_32(y->v+26)>> 2);
  crypto_int64 b11 = load_4_32(y->v+28) >> 7;

  crypto_int64 s0 = a0*b0;
  crypto_int64 s1 = a0*b1 + a1*b0;
  crypto_int64 s2 = a0*b2 + a1*b1 + a2*b0;
  crypto_int64 s3 = a0*b3 + a1*b2 + a2*b1 + a3*b0;
  crypto_int64 s4 = a0*b4 + a1*b3 + a2*b2 + a3*b1 + a4*b0;
  crypto_int64 s5 = a0*b5 + a1*b4 + a2*b3 + a3*b2 + a4*b1 + a5*b0;
  crypto_int64 s6 = a0*b6 + a1*b5 + a2*b4 + a3*b3 + a4*b2 + a5*b1 + a6*b0;
  crypto_int64 s7 = a0*b7 + a1*b6 + a2*b5 + a3*b4 + a4*b3 + a5*b2 + a6*b1 + a7*b0;
  crypto_int64 s8 = a0*b8 + a1*b7 + a2*b6 + a3*b5 + a4*b4 + a5*b3 + a6*b2 + a7*b1 + a8*b0;
  crypto_int64 s9 = a0*b9 + a1*b8 + a2*b7 + a3*b6 + a4*b5 + a5*b4 + a6*b3 + a7*b2 + a8*b1 + a9*b0;
  crypto_int64 s10 =  a0*b10 + a1*b9 + a2*b8 + a3*b7 + a4*b6 + a5*b5 + a6*b4 + a7*b3 + a8*b2 + a9*b1 + a10*b0;
  crypto_int64 s11 =  a0*b11 + a1*b10 + a2*b9 + a3*b8 + a4*b7 + a5*b6 + a6*b5 + a7*b4 + a8*b3 + a9*b2 + a10*b1 + a11*b0;
  crypto_int64 s12 = a1*b11 + a2*b10 + a3*b9 + a4*b8 + a5*b7 + a6*b6 + a7*b5 + a8*b4 + a9*b3 + a10*b2 + a11*b1;
  crypto_int64 s13 = a2*b11 + a3*b10 + a4*b9 + a5*b8 + a6*b7 + a7*b6 + a8*b5 + a9*b4 + a10*b3 + a11*b2;
  crypto_int64 s14 = a3*b11 + a4*b10 + a5*b9 + a6*b8 + a7*b7 + a8*b6 + a9*b5 + a10*b4 + a11*b3;
  crypto_int64 s15 = a4*b11 + a5*b10 + a6*b9 + a7*b8 + a8*b7 + a9*b6 + a10*b5 + a11*b4;
  crypto_int64 s16 = a5*b11 + a6*b10 + a7*b9 + a8*b8 + a9*b7 + a10*b6 + a11*b5;
  crypto_int64 s17 = a6*b11 + a7*b10 + a8*b9 + a9*b8 + a10*b7 + a11*b6;
  crypto_int64 s18 = a7*b11 + a8*b10 + a9*b9 + a10*b8 + a11*b7;
  crypto_int64 s19 = a8*b11 + a9*b10 + a10*b9 + a11*b8;
  crypto_int64 s20 = a9*b11 + a10*b10 + a11*b9;
  crypto_int64 s21 = a10*b11 + a11*b10;
  crypto_int64 s22 = a11 * b11;
  crypto_int64 s23 = 0;

  crypto_uint64 carry[23];

  carry[0] = (s0 + (1 << 20)) >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[2] = (s2 + (1 << 20)) >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[4] = (s4 + (1 << 20)) >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[6] = (s6 + (1 << 20)) >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[8] = (s8 + (1 << 20)) >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[10] = (s10 + (1 << 20)) >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;
  carry[12] = (s12 + (1 << 20)) >> 21;
  s13 += carry[12];
  s12 -= carry[12] << 21;
  carry[14] = (s14 + (1 << 20)) >> 21;
  s15 += carry[14];
  s14 -= carry[14] << 21;
  carry[16] = (s16 + (1 << 20)) >> 21;
  s17 += carry[16];
  s16 -= carry[16] << 21;
  carry[18] = (s18 + (1 << 20)) >> 21;
  s19 += carry[18];
  s18 -= carry[18] << 21;
  carry[20] = (s20 + (1 << 20)) >> 21;
  s21 += carry[20];
  s20 -= carry[20] << 21;
  carry[22] = (s22 + (1 << 20)) >> 21;
  s23 += carry[22];
  s22 -= carry[22] << 21;

  carry[1] = (s1 + (1 << 20)) >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[3] = (s3 + (1 << 20)) >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[5] = (s5 + (1 << 20)) >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[7] = (s7 + (1 << 20)) >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[9] = (s9 + (1 << 20)) >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[11] = (s11 + (1 << 20)) >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;
  carry[13] = (s13 + (1 << 20)) >> 21;
  s14 += carry[13];
  s13 -= carry[13] << 21;
  carry[15] = (s15 + (1 << 20)) >> 21;
  s16 += carry[15];
  s15 -= carry[15] << 21;
  carry[17] = (s17 + (1 << 20)) >> 21;
  s18 += carry[17];
  s17 -= carry[17] << 21;
  carry[19] = (s19 + (1 << 20)) >> 21;
  s20 += carry[19];
  s19 -= carry[19] << 21;
  carry[21] = (s21 + (1 << 20)) >> 21;
  s22 += carry[21];
  s21 -= carry[21] << 21;

  s11 += s23 * 666643;
  s12 += s23 * 470296;
  s13 += s23 * 654183;
  s14 -= s23 * 997805;
  s15 += s23 * 136657;
  s16 -= s23 * 683901;
  s23 = 0;

  s10 += s22 * 666643;
  s11 += s22 * 470296;
  s12 += s22 * 654183;
  s13 -= s22 * 997805;
  s14 += s22 * 136657;
  s15 -= s22 * 683901;
  s22 = 0;

  s9 += s21 * 666643;
  s10 += s21 * 470296;
  s11 += s21 * 654183;
  s12 -= s21 * 997805;
  s13 += s21 * 136657;
  s14 -= s21 * 683901;
  s21 = 0;

  s8 += s20 * 666643;
  s9 += s20 * 470296;
  s10 += s20 * 654183;
  s11 -= s20 * 997805;
  s12 += s20 * 136657;
  s13 -= s20 * 683901;
  s20 = 0;

  s7 += s19 * 666643;
  s8 += s19 * 470296;
  s9 += s19 * 654183;
  s10 -= s19 * 997805;
  s11 += s19 * 136657;
  s12 -= s19 * 683901;
  s19 = 0;

  s6 += s18 * 666643;
  s7 += s18 * 470296;
  s8 += s18 * 654183;
  s9 -= s18 * 997805;
  s10 += s18 * 136657;
  s11 -= s18 * 683901;
  s18 = 0;

  carry[6] = (s6 + (1 << 20)) >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[8] = (s8 + (1 << 20)) >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[10] = (s10 + (1 << 20)) >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;
  carry[12] = (s12 + (1 << 20)) >> 21;
  s13 += carry[12];
  s12 -= carry[12] << 21;
  carry[14] = (s14 + (1 << 20)) >> 21;
  s15 += carry[14];
  s14 -= carry[14] << 21;
  carry[16] = (s16 + (1 << 20)) >> 21;
  s17 += carry[16];
  s16 -= carry[16] << 21;

  carry[7] = (s7 + (1 << 20)) >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[9] = (s9 + (1 << 20)) >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[11] = (s11 + (1 << 20)) >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;
  carry[13] = (s13 + (1 << 20)) >> 21;
  s14 += carry[13];
  s13 -= carry[13] << 21;
  carry[15] = (s15 + (1 << 20)) >> 21;
  s16 += carry[15];
  s15 -= carry[15] << 21;

  s5 += s17 * 666643;
  s6 += s17 * 470296;
  s7 += s17 * 654183;
  s8 -= s17 * 997805;
  s9 += s17 * 136657;
  s10 -= s17 * 683901;
  s17 = 0;

  s4 += s16 * 666643;
  s5 += s16 * 470296;
  s6 += s16 * 654183;
  s7 -= s16 * 997805;
  s8 += s16 * 136657;
  s9 -= s16 * 683901;
  s16 = 0;

  s3 += s15 * 666643;
  s4 += s15 * 470296;
  s5 += s15 * 654183;
  s6 -= s15 * 997805;
  s7 += s15 * 136657;
  s8 -= s15 * 683901;
  s15 = 0;

  s2 += s14 * 666643;
  s3 += s14 * 470296;
  s4 += s14 * 654183;
  s5 -= s14 * 997805;
  s6 += s14 * 136657;
  s7 -= s14 * 683901;
  s14 = 0;

  s1 += s13 * 666643;
  s2 += s13 * 470296;
  s3 += s13 * 654183;
  s4 -= s13 * 997805;
  s5 += s13 * 136657;
  s6 -= s13 * 683901;
  s13 = 0;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry[0] = (s0 + (1 << 20)) >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[2] = (s2 + (1 << 20)) >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[4] = (s4 + (1 << 20)) >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[6] = (s6 + (1 << 20)) >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[8] = (s8 + (1 << 20)) >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[10] = (s10 + (1 << 20)) >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;

  carry[1] = (s1 + (1 << 20)) >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[3] = (s3 + (1 << 20)) >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[5] = (s5 + (1 << 20)) >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[7] = (s7 + (1 << 20)) >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[9] = (s9 + (1 << 20)) >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[11] = (s11 + (1 << 20)) >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry[0] = s0 >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[1] = s1 >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[2] = s2 >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[3] = s3 >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[4] = s4 >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[5] = s5 >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[6] = s6 >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[7] = s7 >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[8] = s8 >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[9] = s9 >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[10] = s10 >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;
  carry[11] = s11 >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry[0] = s0 >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[1] = s1 >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[2] = s2 >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[3] = s3 >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[4] = s4 >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[5] = s5 >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[6] = s6 >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[7] = s7 >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[8] = s8 >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[9] = s9 >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[10] = s10 >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;

  r->v[0] =  0xff & (s0 >> 0);
  r->v[1] =  0xff & (s0 >> 8);
  r->v[2] =  0xff & ((s0 >> 16) | (s1 << 5));
  r->v[3] =  0xff & (s1 >> 3);
  r->v[4] =  0xff & (s1 >> 11);
  r->v[5] =  0xff & ((s1 >> 19) | (s2 << 2));
  r->v[6] =  0xff & (s2 >> 6);
  r->v[7] =  0xff & ((s2 >> 14) | (s3 << 7));
  r->v[8] =  0xff & (s3 >> 1);
  r->v[9] =  0xff & (s3 >> 9);
  r->v[10] = 0xff & ((s3 >> 17) | (s4 << 4));
  r->v[11] = 0xff & (s4 >> 4);
  r->v[12] = 0xff & (s4 >> 12);
  r->v[13] = 0xff & ((s4 >> 20) | (s5 << 1));
  r->v[14] = 0xff & (s5 >> 7);
  r->v[15] = 0xff & ((s5 >> 15) | (s6 << 6));
  r->v[16] = 0xff & (s6 >> 2);
  r->v[17] = 0xff & (s6 >> 10);
  r->v[18] = 0xff & ((s6 >> 18) | (s7 << 3));
  r->v[19] = 0xff & (s7 >> 5);
  r->v[20] = 0xff & (s7 >> 13);
  r->v[21] = 0xff & (s8 >> 0);
  r->v[22] = 0xff & (s8 >> 8);
  r->v[23] = 0xff & ((s8 >> 16) | (s9 << 5));
  r->v[24] = 0xff & (s9 >> 3);
  r->v[25] = 0xff & (s9 >> 11);
  r->v[26] = 0xff & ((s9 >> 19) | (s10 << 2));
  r->v[27] = 0xff & (s10 >> 6);
  r->v[28] = 0xff & ((s10 >> 14) | (s11 << 7));
  r->v[29] = 0xff & (s11 >> 1);
  r->v[30] = 0xff & (s11 >> 9);
  r->v[31] = 0xff & (s11 >> 17);
}

void group_scalar_square(group_scalar *r, const group_scalar *x)
{
  crypto_int64 a0 = 0x1fffff & load_3_32(x->v);
  crypto_int64 a1 = 0x1fffff & (load_4_32(x->v+2) >> 5);
  crypto_int64 a2 = 0x1fffff & (load_3_32(x->v+5) >> 2);
  crypto_int64 a3 = 0x1fffff & (load_4_32(x->v+7) >> 7);
  crypto_int64 a4 = 0x1fffff & (load_4_32(x->v+10)>> 4);
  crypto_int64 a5 = 0x1fffff & (load_3_32(x->v+13)>> 1);
  crypto_int64 a6 = 0x1fffff & (load_4_32(x->v+15)>> 6);
  crypto_int64 a7 = 0x1fffff & (load_3_32(x->v+18)>> 3);
  crypto_int64 a8 = 0x1fffff & load_3_32(x->v+21);
  crypto_int64 a9 = 0x1fffff & (load_4_32(x->v+23)>> 5);
  crypto_int64 a10 =0x1fffff & (load_3_32(x->v+26)>> 2);
  crypto_int64 a11 = load_4_32(x->v+28) >> 7;
  crypto_uint64 carry[23];

  crypto_int64 s0 = a0 * a0;
  crypto_int64 s1 = 2 * a0 * a1;
  crypto_int64 s2 = 2*a0*a2 + a1*a1;
  crypto_int64 s3 = 2 * (a0*a3 + a1*a2);
  crypto_int64 s4 = 2*(a0*a4+a1*a3) + a2*a2;
  crypto_int64 s5 = 2 * (a0*a5 + a1*a4 + a2*a3);
  crypto_int64 s6 = 2*(a0*a6+a1*a5+a2*a4) + a3*a3;
  crypto_int64 s7 = 2 * (a0*a7 + a1*a6 + a2*a5 + a3*a4);
  crypto_int64 s8 = 2*(a0*a8+a1*a7+a2*a6+a3*a5) + a4*a4;
  crypto_int64 s9 = 2 * (a0*a9 + a1*a8 + a2*a7 + a3*a6 + a4*a5);
  crypto_int64 s10 = 2*(a0*a10+a1*a9+a2*a8+a3*a7+a4*a6) + a5*a5;
  crypto_int64 s11 = 2 * (a0*a11 + a1*a10 + a2*a9 + a3*a8 + a4*a7 + a5*a6);
  crypto_int64 s12 = 2*(a1*a11+a2*a10+a3*a9+a4*a8+a5*a7) + a6*a6;
  crypto_int64 s13 = 2 * (a2*a11 + a3*a10 + a4*a9 + a5*a8 + a6*a7);
  crypto_int64 s14 = 2*(a3*a11+a4*a10+a5*a9+a6*a8) + a7*a7;
  crypto_int64 s15 = 2 * (a4*a11 + a5*a10 + a6*a9 + a7*a8);
  crypto_int64 s16 = 2*(a5*a11+a6*a10+a7*a9) + a8*a8;
  crypto_int64 s17 = 2 * (a6*a11 + a7*a10 + a8*a9);
  crypto_int64 s18 = 2*(a7*a11+a8*a10) + a9*a9;
  crypto_int64 s19 = 2 * (a8*a11 + a9*a10);
  crypto_int64 s20 = 2*a9*a11 + a10*a10;
  crypto_int64 s21 = 2 * a10 * a11;
  crypto_int64 s22 = a11 * a11;
  crypto_int64 s23 = 0;

  carry[0] = (s0 + (1 << 20)) >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[2] = (s2 + (1 << 20)) >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[4] = (s4 + (1 << 20)) >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[6] = (s6 + (1 << 20)) >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[8] = (s8 + (1 << 20)) >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[10] = (s10 + (1 << 20)) >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;
  carry[12] = (s12 + (1 << 20)) >> 21;
  s13 += carry[12];
  s12 -= carry[12] << 21;
  carry[14] = (s14 + (1 << 20)) >> 21;
  s15 += carry[14];
  s14 -= carry[14] << 21;
  carry[16] = (s16 + (1 << 20)) >> 21;
  s17 += carry[16];
  s16 -= carry[16] << 21;
  carry[18] = (s18 + (1 << 20)) >> 21;
  s19 += carry[18];
  s18 -= carry[18] << 21;
  carry[20] = (s20 + (1 << 20)) >> 21;
  s21 += carry[20];
  s20 -= carry[20] << 21;
  carry[22] = (s22 + (1 << 20)) >> 21;
  s23 += carry[22];
  s22 -= carry[22] << 21;

  carry[1] = (s1 + (1 << 20)) >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[3] = (s3 + (1 << 20)) >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[5] = (s5 + (1 << 20)) >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[7] = (s7 + (1 << 20)) >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[9] = (s9 + (1 << 20)) >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[11] = (s11 + (1 << 20)) >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;
  carry[13] = (s13 + (1 << 20)) >> 21;
  s14 += carry[13];
  s13 -= carry[13] << 21;
  carry[15] = (s15 + (1 << 20)) >> 21;
  s16 += carry[15];
  s15 -= carry[15] << 21;
  carry[17] = (s17 + (1 << 20)) >> 21;
  s18 += carry[17];
  s17 -= carry[17] << 21;
  carry[19] = (s19 + (1 << 20)) >> 21;
  s20 += carry[19];
  s19 -= carry[19] << 21;
  carry[21] = (s21 + (1 << 20)) >> 21;
  s22 += carry[21];
  s21 -= carry[21] << 21;

  s11 += s23 * 666643;
  s12 += s23 * 470296;
  s13 += s23 * 654183;
  s14 -= s23 * 997805;
  s15 += s23 * 136657;
  s16 -= s23 * 683901;
  s23 = 0;

  s10 += s22 * 666643;
  s11 += s22 * 470296;
  s12 += s22 * 654183;
  s13 -= s22 * 997805;
  s14 += s22 * 136657;
  s15 -= s22 * 683901;
  s22 = 0;

  s9 += s21 * 666643;
  s10 += s21 * 470296;
  s11 += s21 * 654183;
  s12 -= s21 * 997805;
  s13 += s21 * 136657;
  s14 -= s21 * 683901;
  s21 = 0;

  s8 += s20 * 666643;
  s9 += s20 * 470296;
  s10 += s20 * 654183;
  s11 -= s20 * 997805;
  s12 += s20 * 136657;
  s13 -= s20 * 683901;
  s20 = 0;

  s7 += s19 * 666643;
  s8 += s19 * 470296;
  s9 += s19 * 654183;
  s10 -= s19 * 997805;
  s11 += s19 * 136657;
  s12 -= s19 * 683901;
  s19 = 0;

  s6 += s18 * 666643;
  s7 += s18 * 470296;
  s8 += s18 * 654183;
  s9 -= s18 * 997805;
  s10 += s18 * 136657;
  s11 -= s18 * 683901;
  s18 = 0;

  carry[6] = (s6 + (1 << 20)) >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[8] = (s8 + (1 << 20)) >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[10] = (s10 + (1 << 20)) >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;
  carry[12] = (s12 + (1 << 20)) >> 21;
  s13 += carry[12];
  s12 -= carry[12] << 21;
  carry[14] = (s14 + (1 << 20)) >> 21;
  s15 += carry[14];
  s14 -= carry[14] << 21;
  carry[16] = (s16 + (1 << 20)) >> 21;
  s17 += carry[16];
  s16 -= carry[16] << 21;

  carry[7] = (s7 + (1 << 20)) >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[9] = (s9 + (1 << 20)) >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[11] = (s11 + (1 << 20)) >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;
  carry[13] = (s13 + (1 << 20)) >> 21;
  s14 += carry[13];
  s13 -= carry[13] << 21;
  carry[15] = (s15 + (1 << 20)) >> 21;
  s16 += carry[15];
  s15 -= carry[15] << 21;

  s5 += s17 * 666643;
  s6 += s17 * 470296;
  s7 += s17 * 654183;
  s8 -= s17 * 997805;
  s9 += s17 * 136657;
  s10 -= s17 * 683901;
  s17 = 0;

  s4 += s16 * 666643;
  s5 += s16 * 470296;
  s6 += s16 * 654183;
  s7 -= s16 * 997805;
  s8 += s16 * 136657;
  s9 -= s16 * 683901;
  s16 = 0;

  s3 += s15 * 666643;
  s4 += s15 * 470296;
  s5 += s15 * 654183;
  s6 -= s15 * 997805;
  s7 += s15 * 136657;
  s8 -= s15 * 683901;
  s15 = 0;

  s2 += s14 * 666643;
  s3 += s14 * 470296;
  s4 += s14 * 654183;
  s5 -= s14 * 997805;
  s6 += s14 * 136657;
  s7 -= s14 * 683901;
  s14 = 0;

  s1 += s13 * 666643;
  s2 += s13 * 470296;
  s3 += s13 * 654183;
  s4 -= s13 * 997805;
  s5 += s13 * 136657;
  s6 -= s13 * 683901;
  s13 = 0;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry[0] = (s0 + (1 << 20)) >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[2] = (s2 + (1 << 20)) >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[4] = (s4 + (1 << 20)) >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[6] = (s6 + (1 << 20)) >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[8] = (s8 + (1 << 20)) >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[10] = (s10 + (1 << 20)) >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;

  carry[1] = (s1 + (1 << 20)) >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[3] = (s3 + (1 << 20)) >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[5] = (s5 + (1 << 20)) >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[7] = (s7 + (1 << 20)) >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[9] = (s9 + (1 << 20)) >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[11] = (s11 + (1 << 20)) >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry[0] = s0 >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[1] = s1 >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[2] = s2 >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[3] = s3 >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[4] = s4 >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[5] = s5 >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[6] = s6 >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[7] = s7 >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[8] = s8 >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[9] = s9 >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[10] = s10 >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;
  carry[11] = s11 >> 21;
  s12 += carry[11];
  s11 -= carry[11] << 21;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry[0] = s0 >> 21;
  s1 += carry[0];
  s0 -= carry[0] << 21;
  carry[1] = s1 >> 21;
  s2 += carry[1];
  s1 -= carry[1] << 21;
  carry[2] = s2 >> 21;
  s3 += carry[2];
  s2 -= carry[2] << 21;
  carry[3] = s3 >> 21;
  s4 += carry[3];
  s3 -= carry[3] << 21;
  carry[4] = s4 >> 21;
  s5 += carry[4];
  s4 -= carry[4] << 21;
  carry[5] = s5 >> 21;
  s6 += carry[5];
  s5 -= carry[5] << 21;
  carry[6] = s6 >> 21;
  s7 += carry[6];
  s6 -= carry[6] << 21;
  carry[7] = s7 >> 21;
  s8 += carry[7];
  s7 -= carry[7] << 21;
  carry[8] = s8 >> 21;
  s9 += carry[8];
  s8 -= carry[8] << 21;
  carry[9] = s9 >> 21;
  s10 += carry[9];
  s9 -= carry[9] << 21;
  carry[10] = s10 >> 21;
  s11 += carry[10];
  s10 -= carry[10] << 21;

  r->v[0] = 0xff & (s0 >> 0);
  r->v[1] = 0xff & (s0 >> 8);
  r->v[2] = 0xff & ((s0 >> 16) | (s1 << 5));
  r->v[3] = 0xff & (s1 >> 3);
  r->v[4] = 0xff & (s1 >> 11);
  r->v[5] = 0xff & ((s1 >> 19) | (s2 << 2));
  r->v[6] = 0xff & (s2 >> 6);
  r->v[7] = 0xff & ((s2 >> 14) | (s3 << 7));
  r->v[8] = 0xff & (s3 >> 1);
  r->v[9] = 0xff & (s3 >> 9);
  r->v[10] =0xff &  ((s3 >> 17) | (s4 << 4));
  r->v[11] =0xff &  (s4 >> 4);
  r->v[12] =0xff &  (s4 >> 12);
  r->v[13] =0xff &  ((s4 >> 20) | (s5 << 1));
  r->v[14] =0xff &  (s5 >> 7);
  r->v[15] =0xff &  ((s5 >> 15) | (s6 << 6));
  r->v[16] =0xff &  (s6 >> 2);
  r->v[17] =0xff &  (s6 >> 10);
  r->v[18] =0xff &  ((s6 >> 18) | (s7 << 3));
  r->v[19] =0xff &  (s7 >> 5);
  r->v[20] =0xff &  (s7 >> 13);
  r->v[21] =0xff &  (s8 >> 0);
  r->v[22] =0xff &  (s8 >> 8);
  r->v[23] =0xff &  ((s8 >> 16) | (s9 << 5));
  r->v[24] =0xff &  (s9 >> 3);
  r->v[25] =0xff &  (s9 >> 11);
  r->v[26] =0xff &  ((s9 >> 19) | (s10 << 2));
  r->v[27] =0xff &  (s10 >> 6);
  r->v[28] =0xff &  ((s10 >> 14) | (s11 << 7));
  r->v[29] =0xff &  (s11 >> 1);
  r->v[30] =0xff &  (s11 >> 9);
  r->v[31] =0xff &  (s11 >> 17);
}

void group_scalar_invert(group_scalar *r, const group_scalar *x)
{
  group_scalar t0, t1, t2, t3, t4, t5;
  int i;

  group_scalar_square(&t1, x);
  group_scalar_mul(&t2, x, &t1);
  group_scalar_mul(&t0, &t1, &t2);
  group_scalar_square(&t1, &t0);
  group_scalar_square(&t3, &t1);
  group_scalar_mul(&t1, &t2, &t3);
  group_scalar_square(&t2, &t1);
  group_scalar_mul(&t3, &t0, &t2);
  group_scalar_square(&t0, &t3);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t1, &t2, &t0);
  group_scalar_square(&t0, &t1);
  group_scalar_mul(&t1, &t3, &t0);
  group_scalar_square(&t0, &t1);
  group_scalar_square(&t3, &t0);
  group_scalar_mul(&t0, &t1, &t3);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t1, &t3, &t0);
  group_scalar_square(&t0, &t1);
  group_scalar_mul(&t3, &t1, &t0);
  group_scalar_mul(&t0, &t2, &t3);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t1, &t2);
  group_scalar_square(&t3, &t1);
  group_scalar_square(&t4, &t3);
  group_scalar_mul(&t3, &t1, &t4);
  group_scalar_mul(&t1, &t0, &t3);
  group_scalar_mul(&t0, &t2, &t1);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t1, &t2);
  group_scalar_square(&t3, &t1);
  group_scalar_mul(&t1, &t0, &t3);
  group_scalar_square(&t0, &t1);
  group_scalar_square(&t3, &t0);
  group_scalar_mul(&t0, &t1, &t3);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_square(&t1, &t0);
  group_scalar_mul(&t0, &t2, &t1);
  group_scalar_mul(&t1, &t3, &t0);
  group_scalar_square(&t0, &t1);
  group_scalar_square(&t3, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t3, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t3, &t0);
  group_scalar_mul(&t0, &t1, &t3);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t1, &t2, &t0);
  group_scalar_square(&t0, &t1);
  group_scalar_mul(&t4, &t2, &t0);
  group_scalar_square(&t0, &t4);
  group_scalar_square(&t4, &t0);
  group_scalar_mul(&t0, &t1, &t4);
  group_scalar_mul(&t1, &t3, &t0);
  group_scalar_square(&t0, &t1);
  group_scalar_mul(&t3, &t1, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t4, &t0);
  group_scalar_mul(&t0, &t3, &t4);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t2, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t1, &t3, &t0);
  group_scalar_mul(&t0, &t2, &t1);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t1, &t2);
  group_scalar_square(&t3, &t1);
  group_scalar_mul(&t1, &t0, &t3);
  group_scalar_square(&t0, &t1);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_mul(&t0, &t1, &t3);
  group_scalar_square(&t1, &t0);
  group_scalar_square(&t2, &t1);
  group_scalar_mul(&t1, &t0, &t2);
  group_scalar_mul(&t2, &t3, &t1);
  group_scalar_mul(&t1, &t0, &t2);
  group_scalar_mul(&t0, &t2, &t1);
  group_scalar_square(&t2, &t0);
  group_scalar_mul(&t3, &t0, &t2);
  group_scalar_square(&t2, &t3);
  group_scalar_mul(&t3, &t1, &t2);
  group_scalar_mul(&t1, &t0, &t3);
  group_scalar_square(&t0, &t1);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t4, &t2, &t0);
  group_scalar_square(&t0, &t4);
  group_scalar_square(&t4, &t0);
  group_scalar_square(&t5, &t4);
  group_scalar_square(&t4, &t5);
  group_scalar_square(&t5, &t4);
  group_scalar_square(&t4, &t5);
  group_scalar_mul(&t5, &t0, &t4);
  group_scalar_mul(&t0, &t2, &t5);
  group_scalar_mul(&t2, &t3, &t0);
  group_scalar_mul(&t0, &t1, &t2);
  group_scalar_square(&t1, &t0);
  group_scalar_mul(&t3, &t0, &t1);
  group_scalar_square(&t1, &t3);
  group_scalar_mul(&t4, &t0, &t1);
  group_scalar_square(&t1, &t4);
  group_scalar_square(&t4, &t1);
  group_scalar_square(&t1, &t4);
  group_scalar_mul(&t4, &t3, &t1);
  group_scalar_mul(&t1, &t2, &t4);
  group_scalar_square(&t2, &t1);
  group_scalar_square(&t3, &t2);
  group_scalar_square(&t4, &t3);
  group_scalar_mul(&t3, &t2, &t4);
  group_scalar_mul(&t2, &t1, &t3);
  group_scalar_mul(&t3, &t0, &t2);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t2, &t0);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t2, &t1, &t0);
  group_scalar_mul(&t0, &t3, &t2);
  group_scalar_square(&t1, &t0);
  group_scalar_square(&t3, &t1);
  group_scalar_mul(&t4, &t1, &t3);
  group_scalar_square(&t3, &t4);
  group_scalar_square(&t4, &t3);
  group_scalar_mul(&t3, &t1, &t4);
  group_scalar_mul(&t1, &t2, &t3);
  group_scalar_square(&t2, &t1);
  group_scalar_square(&t3, &t2);
  group_scalar_mul(&t2, &t0, &t3);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t3, &t1, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_mul(&t1, &t2, &t0);
  group_scalar_mul(&t0, &t3, &t1);
  group_scalar_square(&t2, &t0);
  group_scalar_square(&t3, &t2);
  group_scalar_square(&t2, &t3);
  group_scalar_square(&t3, &t2);
  group_scalar_mul(&t2, &t1, &t3);
  group_scalar_mul(&t1, &t0, &t2);
  group_scalar_square(&t0, &t1);
  group_scalar_square(&t3, &t0);
  group_scalar_square(&t4, &t3);
  group_scalar_mul(&t3, &t0, &t4);
  group_scalar_mul(&t0, &t1, &t3);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t2, &t0);
  group_scalar_mul(&t0, &t1, &t2);
  group_scalar_square(&t1, &t0);
  group_scalar_mul(&t2, &t3, &t1);
  group_scalar_mul(&t1, &t0, &t2);
  group_scalar_square(&t0, &t1);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t2, &t0);
  group_scalar_mul(&t0, &t1, &t2);
  group_scalar_mul(&t1, &t3, &t0);
  group_scalar_square(&t2, &t1);
  group_scalar_mul(&t3, &t0, &t2);
  group_scalar_mul(&t0, &t1, &t3);
  group_scalar_square(&t1, &t0);
  group_scalar_square(&t2, &t1);
  group_scalar_square(&t4, &t2);
  group_scalar_mul(&t2, &t1, &t4);
  group_scalar_square(&t4, &t2);
  group_scalar_square(&t2, &t4);
  group_scalar_square(&t4, &t2);
  group_scalar_mul(&t2, &t1, &t4);
  group_scalar_mul(&t1, &t3, &t2);
  group_scalar_square(&t2, &t1);
  group_scalar_square(&t3, &t2);
  group_scalar_mul(&t2, &t1, &t3);
  group_scalar_square(&t3, &t2);
  group_scalar_square(&t2, &t3);
  group_scalar_mul(&t3, &t1, &t2);
  group_scalar_mul(&t2, &t0, &t3);
  group_scalar_square(&t0, &t2);
  group_scalar_mul(&t3, &t2, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_square(&t4, &t0);
  group_scalar_mul(&t0, &t3, &t4);
  group_scalar_mul(&t3, &t1, &t0);
  group_scalar_square(&t0, &t3);
  group_scalar_mul(&t1, &t3, &t0);
  group_scalar_mul(&t0, &t2, &t1);
  for(i = 0; i < 126; i++)
    group_scalar_square(&t0, &t0);
  group_scalar_mul(r, &t3, &t0);
}

int  group_scalar_isone(const group_scalar *x)
{
  unsigned long long r;
  int i;
  r = 1-x->v[0];
  for(i=1;i<32;i++)
    r |= x->v[i];
  return 1-((0-r)>>63);
}

int  group_scalar_iszero(const group_scalar *x)
{
  unsigned long long r=0;
  int i;
  for(i=0;i<32;i++)
    r |= x->v[i];
  return 1-((0-r)>>63);
}

int  group_scalar_equals(const group_scalar *x,  const group_scalar *y)
{
  unsigned long long r=0;
  int i;
  for(i=0;i<32;i++)
    r |= (x->v[i] ^ y->v[i]);
  return 1-((0-r)>>63);
}


// Additional functions, not required by API
int  scalar_bitlen(const group_scalar *x)
{
  int i;
  unsigned long long mask;
  int ctr = 256;
  int found = 0;
  int t;
  for(i=31;i>=0;i--)
  {
    for(mask = (1 << 7);mask>0;mask>>=1)
    {
      found = found || (mask & x->v[i]);
      t = ctr - 1;
      ctr = (found * ctr)^((1-found)*t);
    }
  }
  return ctr;
}

void scalar_window3(signed char r[85], const group_scalar *s)
{
  char carry;
  int i;
  for(i=0;i<10;i++)
  {
    r[8*i+0]  =  s->v[3*i+0]       & 7;
    r[8*i+1]  = (s->v[3*i+0] >> 3) & 7;
    r[8*i+2]  = (s->v[3*i+0] >> 6) & 7;
    r[8*i+2] ^= (s->v[3*i+1] << 2) & 7;
    r[8*i+3]  = (s->v[3*i+1] >> 1) & 7;
    r[8*i+4]  = (s->v[3*i+1] >> 4) & 7;
    r[8*i+5]  = (s->v[3*i+1] >> 7) & 7;
    r[8*i+5] ^= (s->v[3*i+2] << 1) & 7;
    r[8*i+6]  = (s->v[3*i+2] >> 2) & 7;
    r[8*i+7]  = (s->v[3*i+2] >> 5) & 7;
  }
  r[8*i+0]  =  s->v[3*i+0]       & 7;
  r[8*i+1]  = (s->v[3*i+0] >> 3) & 7;
  r[8*i+2]  = (s->v[3*i+0] >> 6) & 7;
  r[8*i+2] ^= (s->v[3*i+1] << 2) & 7;
  r[8*i+3]  = (s->v[3*i+1] >> 1) & 7;
  r[8*i+4]  = (s->v[3*i+1] >> 4) & 7;

  /* Making it signed */
  carry = 0;
  for(i=0;i<84;i++)
  {
    r[i] += carry;
    r[i+1] += r[i] >> 3;
    r[i] &= 7;
    carry = r[i] >> 2;
    r[i] -= carry<<3;
  }
  r[84] += carry;
}

void scalar_window4(signed char r[64], const group_scalar *s)
{
  int i;
  for (i = 0; i < 32; i++) {
    r[2*i] = (signed char)(s->v[i] & 15);
    r[2*i+1] = (signed char)((s->v[i] >> 4) & 15);
  }
  signed char carry = 0;
  for (i = 0; i < 63; i++) {
    r[i] += carry;
    carry = (r[i] + 8) >> 4;
    r[i] -= carry << 4;
  }
  r[63] += carry;
}

void scalar_window5(signed char r[51], const group_scalar *s)
{
  char carry;
  int i;
  for(i=0;i<6;i++)
  {
    r[8*i+0]  =  s->v[5*i+0] & 31;
    r[8*i+1]  = (s->v[5*i+0] >> 5) & 31;
    r[8*i+1] ^= (s->v[5*i+1] << 3) & 31;
    r[8*i+2]  = (s->v[5*i+1] >> 2) & 31;
    r[8*i+3]  = (s->v[5*i+1] >> 7) & 31;
    r[8*i+3] ^= (s->v[5*i+2] << 1) & 31;
    r[8*i+4]  = (s->v[5*i+2] >> 4) & 31;
    r[8*i+4] ^= (s->v[5*i+3] << 4) & 31;
    r[8*i+5]  = (s->v[5*i+3] >> 1) & 31;
    r[8*i+6]  = (s->v[5*i+3] >> 6) & 31;
    r[8*i+6] ^= (s->v[5*i+4] << 2) & 31;
    r[8*i+7]  = (s->v[5*i+4] >> 3) & 31;
  }
  r[8*i+0]  =  s->v[5*i+0] & 31;
  r[8*i+1]  = (s->v[5*i+0] >> 5) & 31;
  r[8*i+1] ^= (s->v[5*i+1] << 3) & 31;
  r[8*i+2]  = (s->v[5*i+1] >> 2) & 31;


  /* Making it signed */
  carry = 0;
  for(i=0;i<50;i++)
  {
    r[i] += carry;
    r[i+1] += r[i] >> 5;
    r[i] &= 31;
    carry = r[i] >> 4;
    r[i] -= carry << 5;
  }
  r[50] += carry;
}

void scalar_slide(signed char r[256], const group_scalar *s, int swindowsize)
{
  int i,j,k,b,m=(1<<(swindowsize-1))-1, soplen=256;

  for(i=0;i<32;i++)
  {
    r[8*i+0] =  s->v[i] & 1;
    r[8*i+1] = (s->v[i] >> 1) & 1;
    r[8*i+2] = (s->v[i] >> 2) & 1;
    r[8*i+3] = (s->v[i] >> 3) & 1;
    r[8*i+4] = (s->v[i] >> 4) & 1;
    r[8*i+5] = (s->v[i] >> 5) & 1;
    r[8*i+6] = (s->v[i] >> 6) & 1;
    r[8*i+7] = (s->v[i] >> 7) & 1;
  }

  /* Making it sliding window */
  for (j = 0;j < soplen;++j)
  {
    if (r[j]) {
      for (b = 1;b < soplen - j && b <= 6;++b) {
        if (r[j] + (r[j + b] << b) <= m)
        {
          r[j] += r[j + b] << b; r[j + b] = 0;
        }
        else if (r[j] - (r[j + b] << b) >= -m)
        {
          r[j] -= r[j + b] << b;
          for (k = j + b;k < soplen;++k)
          {
            if (!r[k]) {
              r[k] = 1;
              break;
            }
            r[k] = 0;
          }
        }
        else if (r[j + b])
          break;
      }
    }
  }
}

static crypto_int64 load_3_8(const uint8_t *in)
{
  crypto_int64 result;
  result = (crypto_int64) in[0];
  result |= ((crypto_int64) in[1]) << 8;
  result |= ((crypto_int64) in[2]) << 16;
  return result;
}

static crypto_int64 load_4_8(const uint8_t *in)
{
  crypto_int64 result;
  result = (crypto_int64) in[0];
  result |= ((crypto_int64) in[1]) << 8;
  result |= ((crypto_int64) in[2]) << 16;
  result |= ((crypto_int64) in[3]) << 24;
  return result;
}

void scalar_from64bytes(group_scalar *r, const unsigned char t[64])
{
  crypto_int64 t0 = 0x1FFFFF & load_3_8(t);
  crypto_int64 t1 = 0x1FFFFF & (load_4_8(t+2) >> 5);
  crypto_int64 t2 = 0x1FFFFF & (load_3_8(t+5) >> 2);
  crypto_int64 t3 = 0x1FFFFF & (load_4_8(t+7) >> 7);
  crypto_int64 t4 = 0x1FFFFF & (load_4_8(t+10) >> 4);
  crypto_int64 t5 = 0x1FFFFF & (load_3_8(t+13) >> 1);
  crypto_int64 t6 = 0x1FFFFF & (load_4_8(t+15) >> 6);
  crypto_int64 t7 = 0x1FFFFF & (load_3_8(t+18) >> 3);
  crypto_int64 t8 = 0x1FFFFF & load_3_8(t+21);
  crypto_int64 t9 = 0x1FFFFF & (load_4_8(t+23) >> 5);
  crypto_int64 t10 = 0x1FFFFF & (load_3_8(t+26) >> 2);
  crypto_int64 t11 = 0x1FFFFF & (load_4_8(t+28) >> 7);
  crypto_int64 t12 = 0x1FFFFF & (load_4_8(t+31) >> 4);
  crypto_int64 t13 = 0x1FFFFF & (load_3_8(t+34) >> 1);
  crypto_int64 t14 = 0x1FFFFF & (load_4_8(t+36) >> 6);
  crypto_int64 t15 = 0x1FFFFF & (load_3_8(t+39) >> 3);
  crypto_int64 t16 = 0x1FFFFF & load_3_8(t+42);
  crypto_int64 t17 = 0x1FFFFF & (load_4_8(t+44) >> 5);
  crypto_int64 t18 = 0x1FFFFF & (load_3_8(t+47) >> 2);
  crypto_int64 t19 = 0x1FFFFF & (load_4_8(t+49) >> 7);
  crypto_int64 t20 = 0x1FFFFF & (load_4_8(t+52) >> 4);
  crypto_int64 t21 = 0x1FFFFF & (load_3_8(t+55) >> 1);
  crypto_int64 t22 = 0x1FFFFF & (load_4_8(t+57) >> 6);
  crypto_int64 t23 = (load_4_8(t+60) >> 3);

  t11 += t23 * 666643;
  t12 += t23 * 470296;
  t13 += t23 * 654183;
  t14 -= t23 * 997805;
  t15 += t23 * 136657;
  t16 -= t23 * 683901;
  t23 = 0;

  t10 += t22 * 666643;
  t11 += t22 * 470296;
  t12 += t22 * 654183;
  t13 -= t22 * 997805;
  t14 += t22 * 136657;
  t15 -= t22 * 683901;
  t22 = 0;

  t9 += t21 * 666643;
  t10 += t21 * 470296;
  t11 += t21 * 654183;
  t12 -= t21 * 997805;
  t13 += t21 * 136657;
  t14 -= t21 * 683901;
  t21 = 0;

  t8 += t20 * 666643;
  t9 += t20 * 470296;
  t10 += t20 * 654183;
  t11 -= t20 * 997805;
  t12 += t20 * 136657;
  t13 -= t20 * 683901;
  t20 = 0;

  t7 += t19 * 666643;
  t8 += t19 * 470296;
  t9 += t19 * 654183;
  t10 -= t19 * 997805;
  t11 += t19 * 136657;
  t12 -= t19 * 683901;
  t19 = 0;

  t6 += t18 * 666643;
  t7 += t18 * 470296;
  t8 += t18 * 654183;
  t9 -= t18 * 997805;
  t10 += t18 * 136657;
  t11 -= t18 * 683901;
  t18 = 0;

  crypto_uint64 carry[17];

  carry[6] = (t6 + (1 << 20)) >> 21;
  t7 += carry[6];
  t6 -= carry[6] << 21;
  carry[8] = (t8 + (1 << 20)) >> 21;
  t9 += carry[8];
  t8 -= carry[8] << 21;
  carry[10] = (t10 + (1 << 20)) >> 21;
  t11 += carry[10];
  t10 -= carry[10] << 21;
  carry[12] = (t12 + (1 << 20)) >> 21;
  t13 += carry[12];
  t12 -= carry[12] << 21;
  carry[14] = (t14 + (1 << 20)) >> 21;
  t15 += carry[14];
  t14 -= carry[14] << 21;
  carry[16] = (t16 + (1 << 20)) >> 21;
  t17 += carry[16];
  t16 -= carry[16] << 21;

  carry[7] = (t7 + (1 << 20)) >> 21;
  t8 += carry[7];
  t7 -= carry[7] << 21;
  carry[9] = (t9 + (1 << 20)) >> 21;
  t10 += carry[9];
  t9 -= carry[9] << 21;
  carry[11] = (t11 + (1 << 20)) >> 21;
  t12 += carry[11];
  t11 -= carry[11] << 21;
  carry[13] = (t13 + (1 << 20)) >> 21;
  t14 += carry[13];
  t13 -= carry[13] << 21;
  carry[15] = (t15 + (1 << 20)) >> 21;
  t16 += carry[15];
  t15 -= carry[15] << 21;

  t5 += t17 * 666643;
  t6 += t17 * 470296;
  t7 += t17 * 654183;
  t8 -= t17 * 997805;
  t9 += t17 * 136657;
  t10 -= t17 * 683901;
  t17 = 0;

  t4 += t16 * 666643;
  t5 += t16 * 470296;
  t6 += t16 * 654183;
  t7 -= t16 * 997805;
  t8 += t16 * 136657;
  t9 -= t16 * 683901;
  t16 = 0;

  t3 += t15 * 666643;
  t4 += t15 * 470296;
  t5 += t15 * 654183;
  t6 -= t15 * 997805;
  t7 += t15 * 136657;
  t8 -= t15 * 683901;
  t15 = 0;

  t2 += t14 * 666643;
  t3 += t14 * 470296;
  t4 += t14 * 654183;
  t5 -= t14 * 997805;
  t6 += t14 * 136657;
  t7 -= t14 * 683901;
  t14 = 0;

  t1 += t13 * 666643;
  t2 += t13 * 470296;
  t3 += t13 * 654183;
  t4 -= t13 * 997805;
  t5 += t13 * 136657;
  t6 -= t13 * 683901;
  t13 = 0;

  t0 += t12 * 666643;
  t1 += t12 * 470296;
  t2 += t12 * 654183;
  t3 -= t12 * 997805;
  t4 += t12 * 136657;
  t5 -= t12 * 683901;
  t12 = 0;

  carry[0] = (t0 + (1 << 20)) >> 21;
  t1 += carry[0];
  t0 -= carry[0] << 21;
  carry[2] = (t2 + (1 << 20)) >> 21;
  t3 += carry[2];
  t2 -= carry[2] << 21;
  carry[4] = (t4 + (1 << 20)) >> 21;
  t5 += carry[4];
  t4 -= carry[4] << 21;
  carry[6] = (t6 + (1 << 20)) >> 21;
  t7 += carry[6];
  t6 -= carry[6] << 21;
  carry[8] = (t8 + (1 << 20)) >> 21;
  t9 += carry[8];
  t8 -= carry[8] << 21;
  carry[10] = (t10 + (1 << 20)) >> 21;
  t11 += carry[10];
  t10 -= carry[10] << 21;

  carry[1] = (t1 + (1 << 20)) >> 21;
  t2 += carry[1];
  t1 -= carry[1] << 21;
  carry[3] = (t3 + (1 << 20)) >> 21;
  t4 += carry[3];
  t3 -= carry[3] << 21;
  carry[5] = (t5 + (1 << 20)) >> 21;
  t6 += carry[5];
  t5 -= carry[5] << 21;
  carry[7] = (t7 + (1 << 20)) >> 21;
  t8 += carry[7];
  t7 -= carry[7] << 21;
  carry[9] = (t9 + (1 << 20)) >> 21;
  t10 += carry[9];
  t9 -= carry[9] << 21;
  carry[11] = (t11 + (1 << 20)) >> 21;
  t12 += carry[11];
  t11 -= carry[11] << 21;

  t0 += t12 * 666643;
  t1 += t12 * 470296;
  t2 += t12 * 654183;
  t3 -= t12 * 997805;
  t4 += t12 * 136657;
  t5 -= t12 * 683901;
  t12 = 0;

  carry[0] = t0 >> 21;
  t1 += carry[0];
  t0 -= carry[0] << 21;
  carry[1] = t1 >> 21;
  t2 += carry[1];
  t1 -= carry[1] << 21;
  carry[2] = t2 >> 21;
  t3 += carry[2];
  t2 -= carry[2] << 21;
  carry[3] = t3 >> 21;
  t4 += carry[3];
  t3 -= carry[3] << 21;
  carry[4] = t4 >> 21;
  t5 += carry[4];
  t4 -= carry[4] << 21;
  carry[5] = t5 >> 21;
  t6 += carry[5];
  t5 -= carry[5] << 21;
  carry[6] = t6 >> 21;
  t7 += carry[6];
  t6 -= carry[6] << 21;
  carry[7] = t7 >> 21;
  t8 += carry[7];
  t7 -= carry[7] << 21;
  carry[8] = t8 >> 21;
  t9 += carry[8];
  t8 -= carry[8] << 21;
  carry[9] = t9 >> 21;
  t10 += carry[9];
  t9 -= carry[9] << 21;
  carry[10] = t10 >> 21;
  t11 += carry[10];
  t10 -= carry[10] << 21;
  carry[11] = t11 >> 21;
  t12 += carry[11];
  t11 -= carry[11] << 21;

  t0 += t12 * 666643;
  t1 += t12 * 470296;
  t2 += t12 * 654183;
  t3 -= t12 * 997805;
  t4 += t12 * 136657;
  t5 -= t12 * 683901;
  t12 = 0;

  carry[0] = t0 >> 21;
  t1 += carry[0];
  t0 -= carry[0] << 21;
  carry[1] = t1 >> 21;
  t2 += carry[1];
  t1 -= carry[1] << 21;
  carry[2] = t2 >> 21;
  t3 += carry[2];
  t2 -= carry[2] << 21;
  carry[3] = t3 >> 21;
  t4 += carry[3];
  t3 -= carry[3] << 21;
  carry[4] = t4 >> 21;
  t5 += carry[4];
  t4 -= carry[4] << 21;
  carry[5] = t5 >> 21;
  t6 += carry[5];
  t5 -= carry[5] << 21;
  carry[6] = t6 >> 21;
  t7 += carry[6];
  t6 -= carry[6] << 21;
  carry[7] = t7 >> 21;
  t8 += carry[7];
  t7 -= carry[7] << 21;
  carry[8] = t8 >> 21;
  t9 += carry[8];
  t8 -= carry[8] << 21;
  carry[9] = t9 >> 21;
  t10 += carry[9];
  t9 -= carry[9] << 21;
  carry[10] = t10 >> 21;
  t11 += carry[10];
  t10 -= carry[10] << 21;

  r->v[0]  = (uint8_t)(t0 >> 0);
  r->v[1]  = (uint8_t)(t0 >> 8);
  r->v[2]  = (uint8_t)((t0 >> 16) | (t1 << 5));
  r->v[3]  = (uint8_t)(t1 >> 3);
  r->v[4]  = (uint8_t)(t1 >> 11);
  r->v[5]  = (uint8_t)((t1 >> 19) | (t2 << 2));
  r->v[6]  = (uint8_t)(t2 >> 6);
  r->v[7]  = (uint8_t)((t2 >> 14) | (t3 << 7));
  r->v[8]  = (uint8_t)(t3 >> 1);
  r->v[9]  = (uint8_t)(t3 >> 9);
  r->v[10] = (uint8_t)((t3 >> 17) | (t4 << 4));
  r->v[11] = (uint8_t)(t4 >> 4);
  r->v[12] = (uint8_t)(t4 >> 12);
  r->v[13] = (uint8_t)((t4 >> 20) | (t5 << 1));
  r->v[14] = (uint8_t)(t5 >> 7);
  r->v[15] = (uint8_t)((t5 >> 15) | (t6 << 6));
  r->v[16] = (uint8_t)(t6 >> 2);
  r->v[17] = (uint8_t)(t6 >> 10);
  r->v[18] = (uint8_t)((t6 >> 18) | (t7 << 3));
  r->v[19] = (uint8_t)(t7 >> 5);
  r->v[20] = (uint8_t)(t7 >> 13);
  r->v[21] = (uint8_t)(t8 >> 0);
  r->v[22] = (uint8_t)(t8 >> 8);
  r->v[23] = (uint8_t)((t8 >> 16) | (t9 << 5));
  r->v[24] = (uint8_t)(t9 >> 3);
  r->v[25] = (uint8_t)(t9 >> 11);
  r->v[26] = (uint8_t)((t9 >> 19) | (t10 << 2));
  r->v[27] = (uint8_t)(t10 >> 6);
  r->v[28] = (uint8_t)((t10 >> 14) | (t11 << 7));
  r->v[29] = (uint8_t)(t11 >> 1);
  r->v[30] = (uint8_t)(t11 >> 9);
  r->v[31] = (uint8_t)(t11 >> 17);
}

// Computes the w=5 w-NAF of s and store it into naf.
//
// naf is assumed to be zero-initialized and the highest three bits of s
// have to be cleared.
void scalar_wnaf5(signed char r[256], const group_scalar *s)
{
  uint64_t x[5] = { 0 };
  int i;
  for (i = 0; i < 4; i++)
    x[i] = load_8u(&s->v[8*i]);
  uint16_t pos = 0;
  uint64_t carry = 0;
  while (pos < 256) {
    uint16_t idx = pos / 64;
    uint16_t bit_idx = pos % 64;
    uint64_t bit_buf;

    if (bit_idx < 59)
      bit_buf = x[idx] >> bit_idx;
    else
      bit_buf = (x[idx] >> bit_idx) | (x[1+idx] << (64 - bit_idx));

    uint64_t window = carry + (bit_buf & 31);
    if ((window & 1) == 0) {
      pos++;
      continue;
    }

    if (window < 16) {
      carry = 0;
      r[pos] = (signed char)(window);
    } else {
      carry = 1;
      r[pos] = (signed char)(window) - 32;
    }

    pos += 5;
  }
}

void shortscalar_hashfromstr(group_scalar *r, const unsigned char *s, unsigned long long slen)
{
  unsigned char h[64];
  crypto_hash_sha512(h, s, slen);
  int i;
  for (i = 0; i < 16; i++)
    r->v[i] = h[i];
  for (i = 16; i < 32; i++)
    r->v[i] = 0;
}

void scalar_hashfromstr(group_scalar *r, const unsigned char *s, unsigned long long slen)
{
  unsigned char h[64];
  crypto_hash_sha512(h, s, slen);
  scalar_from64bytes(r, h);
}
