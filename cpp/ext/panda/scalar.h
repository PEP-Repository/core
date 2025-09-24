#ifndef GROUP_SCALAR_H
#define GROUP_SCALAR_H

#define GROUP_SCALAR_PACKEDBYTES 32
#include <inttypes.h>

typedef struct 
{
  uint32_t v[32]; 
}
group_scalar;

extern const group_scalar group_scalar_zero;
extern const group_scalar group_scalar_one;

int group_scalar_unpack(group_scalar *r, const unsigned char x[GROUP_SCALAR_PACKEDBYTES]);
void group_scalar_pack(unsigned char s[GROUP_SCALAR_PACKEDBYTES], const group_scalar *r);

void group_scalar_setzero(group_scalar *r);
void group_scalar_setone(group_scalar *r);
//void group_scalar_setrandom(group_scalar *r); // Removed to avoid dependency on platform specific randombytes
void group_scalar_add(group_scalar *r, const group_scalar *x, const group_scalar *y);
void group_scalar_sub(group_scalar *r, const group_scalar *x, const group_scalar *y);
void group_scalar_negate(group_scalar *r, const group_scalar *x);
void group_scalar_mul(group_scalar *r, const group_scalar *x, const group_scalar *y);
void group_scalar_square(group_scalar *r, const group_scalar *x);
void group_scalar_invert(group_scalar *r, const group_scalar *x);

int  group_scalar_isone(const group_scalar *x);
int  group_scalar_iszero(const group_scalar *x);
int  group_scalar_equals(const group_scalar *x,  const group_scalar *y);


// Additional functions, not required by API
int  scalar_bitlen(const group_scalar *x);
void scalar_window3(signed char r[85], const group_scalar *x);
void scalar_window4(signed char r[64], const group_scalar *s);
void scalar_window5(signed char r[51], const group_scalar *s);
void scalar_slide(signed char r[256], const group_scalar *s, int swindowsize);
void scalar_wnaf5(signed char r[256], const group_scalar *s);

void scalar_from64bytes(group_scalar *r, const unsigned char h[64]);
void scalar_hashfromstr(group_scalar *r, const unsigned char *s, unsigned long long slen);

// Derives a half-length scalar from data using a hash.
// Half-length scalars are only safe for very specific situations.
void shortscalar_hashfromstr(group_scalar *r, const unsigned char *s, unsigned long long slen);
#endif
