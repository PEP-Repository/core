#ifndef GROUP_GE_H 
#define GROUP_GE_H

#include "fe25519.h"
#include "scalar.h"

#define GROUP_GE_PACKEDBYTES 32

typedef struct
{	
	fe25519 x;
	fe25519 y;
	fe25519 z;
	fe25519 t;
} group_ge;

// Precomputed group element. (Affine Niels representation.)
typedef struct
{
  fe25519 yPlusX;
  fe25519 yMinusX;
  fe25519 xY2D;
} group_niels;

typedef struct
{
  group_niels v[256];
} group_scalarmult_table;

extern const group_ge group_ge_neutral;

void group_scalarmult_table_compute(group_scalarmult_table* table, const group_ge* x);
void group_ge_scalarmult_table(group_ge *r, const group_scalarmult_table *table, const group_scalar *s);
void group_ge_scalarmult_table_publicinputs(group_ge *r, const group_scalarmult_table *table, const group_scalar *s);

// Constant-time versions
int  group_ge_unpack(group_ge *r, const unsigned char x[GROUP_GE_PACKEDBYTES]);
void group_ge_pack(unsigned char r[GROUP_GE_PACKEDBYTES], const group_ge *x);

void group_ge_hashfromstr(group_ge *r, const unsigned char *s, unsigned long long slen);
void group_ge_add(group_ge *r, const group_ge *x, const group_ge *y);
void group_ge_double(group_ge *r, const group_ge *x);
void group_ge_negate(group_ge *r, const group_ge *x);
void group_ge_scalarmult(group_ge *r, const group_ge *x, const group_scalar *s);
void group_ge_scalarmult_base(group_ge *r, const group_scalar *s);
void group_ge_multiscalarmult(group_ge *r, const group_ge *x, const group_scalar *s, unsigned long long xlen);
int  group_ge_equals(const group_ge *x, const group_ge *y);
int  group_ge_isneutral(const group_ge *x);

// Non-constant-time versions
void group_ge_hashfromstr_publicinputs(group_ge *r, const unsigned char *s, unsigned long long slen);
void group_ge_add_publicinputs(group_ge *r, const group_ge *x, const group_ge *y);
void group_ge_double_publicinputs(group_ge *r, const group_ge *x);
void group_ge_negate_publicinputs(group_ge *r, const group_ge *x);
void group_ge_scalarmult_publicinputs(group_ge *r, const group_ge *x, const group_scalar *s);
void group_ge_scalarmult_base_publicinputs(group_ge *r, const group_scalar *s);
void group_ge_multiscalarmult_publicinputs(group_ge *r, const group_ge *x, const group_scalar *s, unsigned long long xlen);
int  group_ge_equals_publicinputs(const group_ge *x, const group_ge *y);
int  group_ge_isneutral_publicinputs(const group_ge *x);

// Not required by API
//void ge_print(const group_ge *x);

#endif
