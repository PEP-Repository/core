#include "scalar.h"
#include "group.h"

#include "crypto_hash_sha512.h"
#include "crypto_ints.h"

/*
 * Arithmetic on the twisted Edwards curve -x^2 + y^2 = 1 + dx^2y^2
 * with d = -(121665/121666) = 37095705934669439343138083508754565189542113879843219016388785533085940283555
 * Base point: (15112221349535400772501151409588531511454012693041857206046113283949847762202,46316835694926478169428394003475163141307993866256225615783033603165251855960);
 */


#ifdef FE25519_RADIX51
  static const fe25519 ge25519_ecd = {{929955233495203, 466365720129213, 1662059464998953, 2033849074728123, 1442794654840575}};
  static const fe25519 ge25519_ec2d = {{1859910466990425, 932731440258426, 1072319116312658, 1815898335770999, 633789495995903}};
  static const fe25519 ge25519_magic = {{1972891073822467, 1430154612583622, 2243686579258279, 473840635492096, 133279003116800}};
  const group_ge group_ge_neutral = {{{0,0,0,0,0}}, {{1,0,0,0,0}}, {{1,0,0,0,0}}, {{0,0,0,0,0}}};
#else
static const fe25519 ge25519_ecd = {{-10913610, 13857413, -15372611, 6949391, 114729, -8787816, -6275908, -3247719, -18696448, -12055116}};
static const fe25519 ge25519_ec2d = {{-21827239, -5839606, -30745221, 13898782, 229458, 15978800, -12551817, -6495438, 29715968, 9444199}};
static const fe25519 ge25519_magic = {{-6111485, -4156064, 27798727, -12243468, 25904040, -120897, -20826367, 7060776, -6093568, 1986012}};
const group_ge group_ge_neutral = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
                                    {{1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
                                    {{1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
                                    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};
#endif

#define ge25519_p3 group_ge

typedef struct
{
  fe25519 x;
  fe25519 z;
  fe25519 y;
  fe25519 t;
} ge25519_p1p1;

typedef struct
{
  fe25519 x;
  fe25519 y;
  fe25519 z;
} ge25519_p2;

typedef struct
{
  fe25519 x;
  fe25519 y;
} ge25519_aff;


/* Multiples of the base point in affine representation */
static const group_scalarmult_table ge25519_base_table = {{
#include "ge25519_base.data"
}};


static void p1p1_to_p2(ge25519_p2 *r, const ge25519_p1p1 *p)
{
  fe25519_mul(&r->x, &p->x, &p->t);
  fe25519_mul(&r->y, &p->y, &p->z);
  fe25519_mul(&r->z, &p->z, &p->t);
}

static void p1p1_to_p3(ge25519_p3 *r, const ge25519_p1p1 *p)
{
  p1p1_to_p2((ge25519_p2 *)r, p);
  fe25519_mul(&r->t, &p->x, &p->y);
}

static void sub_p1p1(ge25519_p1p1 *r, const ge25519_p3 *p, const ge25519_p3 *q)
{
  fe25519 a, b, c, d, t;

  fe25519_sub(&a, &p->y, &p->x);
  fe25519_add(&t, &q->y, &q->x);
  fe25519_mul(&a, &a, &t);
  fe25519_add(&b, &p->x, &p->y);
  fe25519_sub(&t, &q->y, &q->x);
  fe25519_mul(&b, &b, &t);
  fe25519_mul(&c, &p->t, &q->t);
  fe25519_mul(&c, &c, &ge25519_ec2d);
  fe25519_mul(&d, &p->z, &q->z);
  fe25519_add(&d, &d, &d);
  fe25519_sub(&r->x, &b, &a);
  fe25519_add(&r->t, &d, &c);
  fe25519_sub(&r->z, &d, &c);
  fe25519_add(&r->y, &b, &a);
}


static void add_p1p1(ge25519_p1p1 *r, const ge25519_p3 *p, const ge25519_p3 *q)
{
  fe25519 a, b, c, d, t;

  fe25519_sub(&a, &p->y, &p->x); /* A = (Y1-X1)*(Y2-X2) */
  fe25519_sub(&t, &q->y, &q->x);
  fe25519_mul(&a, &a, &t);
  fe25519_add(&b, &p->x, &p->y); /* B = (Y1+X1)*(Y2+X2) */
  fe25519_add(&t, &q->x, &q->y);
  fe25519_mul(&b, &b, &t);
  fe25519_mul(&c, &p->t, &q->t); /* C = T1*k*T2 */
  fe25519_mul(&c, &c, &ge25519_ec2d);
  fe25519_mul(&d, &p->z, &q->z); /* D = Z1*2*Z2 */
  fe25519_add(&d, &d, &d);
  fe25519_sub(&r->x, &b, &a); /* E = B-A */
  fe25519_sub(&r->t, &d, &c); /* F = D-C */
  fe25519_add(&r->z, &d, &c); /* G = D+C */
  fe25519_add(&r->y, &b, &a); /* H = B+A */
}

/* See http://www.hyperelliptic.org/EFD/g1p/auto-twisted-extended-1.html#doubling-dbl-2008-hwcd */
static void dbl_p1p1(ge25519_p1p1 *r, const ge25519_p2 *p)
{
  fe25519 a,b,c,d;
  fe25519_square(&a, &p->x);
  fe25519_square(&b, &p->y);
  fe25519_square_double(&c, &p->z);
  fe25519_neg(&d, &a);

  fe25519_add(&r->x, &p->x, &p->y);
  fe25519_square(&r->x, &r->x);
  fe25519_sub(&r->x, &r->x, &a);
  fe25519_sub(&r->x, &r->x, &b);
  fe25519_add(&r->z, &d, &b);
  fe25519_sub(&r->t, &r->z, &c);
  fe25519_sub(&r->y, &d, &b);
}

static unsigned char equal(signed char b, signed char c)
{
  unsigned char ub = (unsigned char)b;
  unsigned char uc = (unsigned char)c;
  unsigned char x = ub ^ uc; /* 0: yes; 1..255: no */
  crypto_uint32 y = x; /* 0: yes; 1..255: no */
  y -= 1; /* 4294967295: yes; 0..254: no */
  y >>= 31; /* 1: yes; 0: no */
  return (unsigned char)y;
}

static unsigned char negative(signed char b)
{
  unsigned long long x = (unsigned long long)b; /* 18446744073709551361..18446744073709551615: yes; 0..255: no */
  x >>= 63; /* 1: yes; 0: no */
  return (unsigned char)x;
}

static void choose_t(group_ge *t, const group_ge *pre, signed char b)
{
  fe25519 v;
  signed char j;
  unsigned char c;

  *t = pre[0];
  for(j=1;j<=16;j++)
  {
    c = equal(b,j) | equal(-b,j);
    fe25519_cmov(&t->x, &pre[j].x,c);
    fe25519_cmov(&t->y, &pre[j].y,c);
    fe25519_cmov(&t->z, &pre[j].z,c);
    fe25519_cmov(&t->t, &pre[j].t,c);
  }
  fe25519_neg(&v, &t->x);
  fe25519_cmov(&t->x, &v, negative(b));
  fe25519_neg(&v, &t->t);
  fe25519_cmov(&t->t, &v, negative(b));
}


// ==================================================================================
//                                    API FUNCTIONS
// ==================================================================================


int group_ge_unpack(group_ge *r, const unsigned char x[GROUP_GE_PACKEDBYTES])
{
  fe25519 s, s2, chk, yden, ynum, yden2, xden2, isr, xdeninv, ydeninv, t;
  int ret;
  unsigned char b;

  fe25519_unpack(&s, x);

  /* s = cls.bytesToGf(s,mustBePositive=True) */
  ret = fe25519_isnegative(&s);

  /* yden     = 1-a*s^2    // 1+s^2 */
  /* ynum     = 1+a*s^2    // 1-s^2 */
  fe25519_square(&s2, &s);
  fe25519_add(&yden,&fe25519_one,&s2);
  fe25519_sub(&ynum,&fe25519_one,&s2);

  /* yden_sqr = yden^2 */
  /* xden_sqr = a*d*ynum^2 - yden_sqr */
  fe25519_square(&yden2, &yden);
  fe25519_square(&xden2, &ynum);
  fe25519_mul(&xden2, &xden2, &ge25519_ecd); // d*ynum^2
  fe25519_add(&xden2, &xden2, &yden2); // d*ynum2+yden2
  fe25519_neg(&xden2, &xden2); // -d*ynum2-yden2

  /* isr = isqrt(xden_sqr * yden_sqr) */
  fe25519_mul(&t, &xden2, &yden2);
  fe25519_invsqrt(&isr, &t);

  //Check inverse square root!
  fe25519_square(&chk, &isr);
  fe25519_mul(&chk, &chk, &t);

  ret |= !fe25519_isone(&chk);

  /* xden_inv = isr * yden */
  fe25519_mul(&xdeninv, &isr, &yden);


  /* yden_inv = xden_inv * isr * xden_sqr */
  fe25519_mul(&ydeninv, &xdeninv, &isr);
  fe25519_mul(&ydeninv, &ydeninv, &xden2);

  /* x = 2*s*xden_inv */
  fe25519_mul(&r->x, &s, &xdeninv);
  fe25519_double(&r->x, &r->x);

  /* if negative(x): x = -x */
  b = (unsigned char)fe25519_isnegative(&r->x);
  fe25519_neg(&t, &r->x);
  fe25519_cmov(&r->x, &t, b);


  /* y = ynum * yden_inv */
  fe25519_mul(&r->y, &ynum, &ydeninv);

  r->z = fe25519_one;

  /* if cls.cofactor==8 and (negative(x*y) or y==0):
       raise InvalidEncodingException("x*y is invalid: %d, %d" % (x,y)) */
  fe25519_mul(&r->t, &r->x, &r->y);
  ret |= fe25519_isnegative(&r->t);
  ret |= fe25519_iszero(&r->y);


  // Zero all coordinates of point for invalid input; produce invalid point
  fe25519_cmov(&r->x, &fe25519_zero, (unsigned char)ret);
  fe25519_cmov(&r->y, &fe25519_zero, (unsigned char)ret);
  fe25519_cmov(&r->z, &fe25519_zero, (unsigned char)ret);
  fe25519_cmov(&r->t, &fe25519_zero, (unsigned char)ret);

  return -ret;
}

void group_ge_pack(unsigned char r[GROUP_GE_PACKEDBYTES], const group_ge *x)
{
  fe25519 d, u1, u2, isr, i1, i2, zinv, deninv, nx, ny, s;
  unsigned char b;

  /* u1    = mneg*(z+y)*(z-y) */
  fe25519_add(&d, &x->z, &x->y);
  fe25519_sub(&u1, &x->z, &x->y);
  fe25519_mul(&u1, &u1, &d);

  /* u2    = x*y # = t*z */
  fe25519_mul(&u2, &x->x, &x->y);

  /* isr   = isqrt(u1*u2^2) */
  fe25519_square(&isr, &u2);
  fe25519_mul(&isr, &isr, &u1);
  fe25519_invsqrt(&isr, &isr);

  /* i1    = isr*u1 # sqrt(mneg*(z+y)*(z-y))/(x*y) */
  fe25519_mul(&i1, &isr, &u1);

  /* i2    = isr*u2 # 1/sqrt(a*(y+z)*(y-z)) */
  fe25519_mul(&i2, &isr, &u2);

  /* z_inv = i1*i2*t # 1/z */
  fe25519_mul(&zinv, &i1, &i2);
  fe25519_mul(&zinv, &zinv, &x->t);

  /* if negative(t*z_inv):
       x,y = y*self.i,x*self.i
       den_inv = self.magic * i1 */
  fe25519_mul(&d, &zinv, &x->t);
  b = !fe25519_isnegative(&d);

  fe25519_mul(&nx, &x->y, &fe25519_sqrtm1);
  fe25519_mul(&ny, &x->x, &fe25519_sqrtm1);
  fe25519_mul(&deninv, &ge25519_magic, &i1);

  fe25519_cmov(&nx, &x->x, b);
  fe25519_cmov(&ny, &x->y, b);
  fe25519_cmov(&deninv, &i2, b);

  /* if negative(x*z_inv): y = -y */
  fe25519_mul(&d, &nx, &zinv);
  b = (unsigned char)fe25519_isnegative(&d);
  fe25519_neg(&d, &ny);
  fe25519_cmov(&ny, &d, b);

  /* s = (z-y) * den_inv */
  fe25519_sub(&s, &x->z, &ny);
  fe25519_mul(&s, &s, &deninv);

  /* return self.gfToBytes(s,mustBePositive=True) */
  b = (unsigned char)fe25519_isnegative(&s);
  fe25519_neg(&d, &s);
  fe25519_cmov(&s, &d, b);

  fe25519_pack(r, &s);
}

void group_ge_add(group_ge *r, const group_ge *x, const group_ge *y)
{
  ge25519_p1p1 t;
  add_p1p1(&t, x, y);
  p1p1_to_p3(r,&t);
}

void group_ge_double(group_ge *r, const group_ge *x)
{
  ge25519_p1p1 t;
  dbl_p1p1(&t, (ge25519_p2 *)x);
  p1p1_to_p3(r,&t);
}

void group_ge_negate(group_ge *r, const group_ge *x)
{
  fe25519_neg(&r->x, &x->x);
  r->y = x->y;
  r->z = x->z;
  fe25519_neg(&r->t, &x->t);
}

void group_ge_scalarmult(group_ge *r, const group_ge *x, const group_scalar *s)
{
  group_ge precomp[17],t;
  int i, j;
  signed char win5[51];

  scalar_window5(win5, s);

  //precomputation:
  precomp[0] = group_ge_neutral;
  precomp[1] = *x;
  for (i = 2; i < 16; i+=2)
  {
    group_ge_double(precomp+i,precomp+i/2);
    group_ge_add(precomp+i+1,precomp+i,precomp+1);
  }
  group_ge_double(precomp+16,precomp+8);


  *r = group_ge_neutral;
  for (i = 50; i >= 0; i--)
  {
    // set r to 32 * r
    ge25519_p1p1 r_p1p1;
    ge25519_p2 r_p2;
    dbl_p1p1(&r_p1p1, (ge25519_p2 *)r);
    for (j = 0; j < 4; j++) {
      p1p1_to_p2(&r_p2, &r_p1p1);
      dbl_p1p1(&r_p1p1, &r_p2);
    }
    p1p1_to_p3(r, &r_p1p1);

    choose_t(&t, precomp, win5[i]);
    group_ge_add(r, r, &t);
  }
}

void group_ge_scalarmult_base(group_ge *r, const group_scalar *s)
{
  group_ge_scalarmult_table(r, &ge25519_base_table, s);
}

void group_ge_multiscalarmult(group_ge *r, const group_ge *x, const group_scalar *s, unsigned long long xlen)
{
  //XXX: Use Strauss
  unsigned long long i;
  group_ge t;
  *r = group_ge_neutral;
  for(i=0;i<xlen;i++)
  {
    group_ge_scalarmult(&t,x+i,s+i);
    group_ge_add(r,r,&t);
  }
}

int  group_ge_equals(const group_ge *x, const group_ge *y)
{
  fe25519 x1y2, x2y1, x1x2, y1y2;
  int r;

  fe25519_mul(&x1y2, &x->x, &y->y);
  fe25519_mul(&x2y1, &y->x, &x->y);

  r =  fe25519_iseq(&x1y2, &x2y1);

  fe25519_mul(&x1x2, &x->x, &y->x);
  fe25519_mul(&y1y2, &x->y, &y->y);

  r |=  fe25519_iseq(&x1x2, &y1y2);

  return r;
}

int  group_ge_isneutral(const group_ge *x)
{
  int r;
  group_ge t;

  // double three times for decaf8
  group_ge_double(&t, x);
  group_ge_double(&t, &t);
  group_ge_double(&t, &t);

  r = 1-fe25519_iszero(&t.x);
  r |= 1-fe25519_iseq(&t.y, &t.z);
  return 1-r;
}

void group_ge_add_publicinputs(group_ge *r, const group_ge *x, const group_ge *y)
{
  group_ge_add(r,x,y);
}

void group_ge_double_publicinputs(group_ge *r, const group_ge *x)
{
  group_ge_double(r,x);
}

void group_ge_negate_publicinputs(group_ge *r, const group_ge *x)
{
  group_ge_negate(r,x);
}

void group_ge_scalarmult_publicinputs(group_ge *r, const group_ge *q, const group_scalar *s)
{
  group_ge dblQ;
  group_ge lut[8];
  group_ge ret = group_ge_neutral;

  group_ge_double(&dblQ, q);
  lut[0] = *q;
  int i;

  for (i = 1; i < 8; i++)
    group_ge_add(&lut[i], &lut[i-1], &dblQ);

  signed char naf[256] = { 0 };
  scalar_wnaf5(naf, s);

  // skip trailing zeroes
  i = 255;
  for (; i >= 0; i--)
    if (naf[i] != 0)
      break;

  // Corner-case: s is zero
  if (i == -1) {
    *r = ret;
    return;
  }

  ge25519_p2 pp;
  ge25519_p1p1 cp;

  for (;;) {
    if (naf[i] > 0)
      add_p1p1(&cp, &ret, &lut[(naf[i]+1)/2-1]);
    else
      sub_p1p1(&cp, &ret, &lut[(1-naf[i])/2-1]);

    if (i == 0) {
      p1p1_to_p3(&ret, &cp);
      break;
    }

    i--;
    p1p1_to_p2(&pp, &cp);
    dbl_p1p1(&cp, &pp);

    // Find next non-zero digit
    for (;;) {
      if (i == 0 || naf[i] != 0)
        break;

      i--;
      p1p1_to_p2(&pp, &cp);
      dbl_p1p1(&cp, &pp);
    }
    p1p1_to_p3(&ret, &cp);

    if (naf[i] == 0)
      break;
  }
  *r = ret;
}


void group_ge_scalarmult_base_publicinputs(group_ge *r, const group_scalar *s)
{
  group_ge_scalarmult_table_publicinputs(r, &ge25519_base_table, s);
}

void group_ge_multiscalarmult_publicinputs(group_ge *r, const group_ge *x, const group_scalar *s, unsigned long long xlen)
{
  //XXX: Use Bos-Coster (and something else for small values of xlen)
  group_ge_multiscalarmult(r,x,s,xlen);
}

int  group_ge_equals_publicinputs(const group_ge *x, const group_ge *y)
{
  return group_ge_equals(x,y);
}

int  group_ge_isneutral_publicinputs(const group_ge *x)
{
  return group_ge_isneutral(x);
}

static void group_ge_from_jacobi_quartic(group_ge *x, const fe25519 *s, const fe25519 *t)
{
    ge25519_p1p1 res;
    fe25519 s2;

    fe25519_square(&s2, s);

    // Set x to 2 * s * 1/sqrt(-d-1)
    fe25519_double(&res.x, s);
    fe25519_mul(&res.x, &res.x, &ge25519_magic);

    // Set z to t
    res.z = *t;

    // Set y to 1-s^2
    fe25519_sub(&res.y, &fe25519_one, &s2);

    // Set t to 1+s^2
    fe25519_add(&res.t, &fe25519_one, &s2);
    p1p1_to_p3(x, &res);
}

// Compute the point corresponding to the scalar r0 in the
// Elligator2 encoding adapted to Ristretto.
void group_ge_elligator(group_ge *x, const fe25519 *r0)
{
    fe25519 r, rPlusD, rPlusOne, ecd2, D, N, ND, sqrt, twiddle, sgn;
    fe25519 s, t, dMinusOneSquared, rSubOne, r0i, sNeg;
    int b;

    fe25519_mul(&r0i, r0, &fe25519_sqrtm1);
    fe25519_mul(&r, r0, &r0i);

    fe25519_add(&rPlusD, &ge25519_ecd, &r);
    fe25519_mul(&D, &ge25519_ecd, &r);
    fe25519_add(&D, &D, &fe25519_one);
    fe25519_mul(&D, &D, &rPlusD);
    fe25519_neg(&D, &D);

    fe25519_square(&ecd2, &ge25519_ecd);
    fe25519_sub(&N, &ecd2, &fe25519_one);
    fe25519_neg(&N, &N); // TODO add -(d^2-1) as a constant
    fe25519_add(&rPlusOne, &r, &fe25519_one);
    fe25519_mul(&N, &N, &rPlusOne);

    fe25519_mul(&ND, &N, &D);
    b = fe25519_invsqrti(&sqrt, &ND);
    fe25519_abs(&sqrt, &sqrt);

    fe25519_setone(&twiddle);
    fe25519_cmov(&twiddle, &r0i, (unsigned char)(1 - b));
    fe25519_setone(&sgn);
    fe25519_cmov(&sgn, &fe25519_m1, (unsigned char)(1 - b));
    fe25519_mul(&sqrt, &sqrt, &twiddle);

    fe25519_mul(&s, &sqrt, &N);

    fe25519_neg(&t, &sgn);
    fe25519_mul(&t, &sqrt, &t);
    fe25519_mul(&t, &s, &t);
    fe25519_sub(&dMinusOneSquared, &ge25519_ecd, &fe25519_one);
    fe25519_square(&dMinusOneSquared, &dMinusOneSquared); // TODO make constant
    fe25519_mul(&t, &dMinusOneSquared, &t);
    fe25519_sub(&rSubOne, &r, &fe25519_one);
    fe25519_mul(&t, &rSubOne, &t);
    fe25519_sub(&t, &t, &fe25519_one);

    fe25519_neg(&sNeg, &s);
    fe25519_cmov(&s, &sNeg, (unsigned char)(fe25519_isnegative(&s) == b));

    group_ge_from_jacobi_quartic(x, &s, &t);
}

// Create a group element from a string by hashing it and then using
// the elligator2 encoding adapted to Ristretto.
void group_ge_hashfromstr(group_ge *r, const unsigned char *s, unsigned long long slen)
{
    unsigned char h[64];
    fe25519 fe;
    crypto_hash_sha512(h, s, slen);
    fe25519_unpack(&fe, h);
    group_ge_elligator(r, &fe);
}

static void niels_setone(group_niels* r)
{
    fe25519_setone(&r->yMinusX);
    fe25519_setone(&r->yPlusX);
    fe25519_setzero(&r->xY2D);
}

static void niels_cmov(group_niels *r, const group_niels *p, unsigned char b)
{
    fe25519_cmov(&r->yMinusX, &p->yMinusX, b);
    fe25519_cmov(&r->yPlusX, &p->yPlusX, b);
    fe25519_cmov(&r->xY2D, &p->xY2D, b);
}

static void niels_neg(group_niels *r, const group_niels* q)
{
    r->yPlusX = q->yMinusX;
    r->yMinusX = q->yPlusX;
    fe25519_neg(&r->xY2D, &q->xY2D);
}

static void niels_mixadd(ge25519_p1p1* p, const group_ge* q, const group_niels* r)
{
    fe25519 t0;

    fe25519_add(&p->x, &q->y, &q->x);
    fe25519_sub(&p->y, &q->y, &q->x);
    fe25519_mul(&p->z, &p->x, &r->yPlusX);
    fe25519_mul(&p->y, &p->y, &r->yMinusX);
    fe25519_mul(&p->t, &r->xY2D, &q->t);
    fe25519_double(&t0, &q->z);
    fe25519_sub(&p->x, &p->z, &p->y);
    fe25519_add(&p->y, &p->z, &p->y);
    fe25519_add(&p->z, &t0, &p->t);
    fe25519_sub(&p->t, &t0, &p->t);
}

static void niels_set_p3(group_niels *r, const group_ge* q)
{
    fe25519 x, y, zInv;

    fe25519_invert(&zInv, &q->z);
    fe25519_mul(&x, &q->x, &zInv);
    fe25519_mul(&y, &q->y, &zInv);
    fe25519_add(&r->yPlusX, &y, &x);
    fe25519_set_reduced(&r->yPlusX, &r->yPlusX);
    fe25519_sub(&r->yMinusX, &y, &x);
    fe25519_set_reduced(&r->yMinusX, &r->yMinusX);
    fe25519_mul(&r->xY2D, &y, &x);
    fe25519_mul(&r->xY2D, &r->xY2D, &ge25519_ec2d);
}

void group_scalarmult_table_compute(group_scalarmult_table* table, const group_ge* x)
{
    group_ge c, cp;
    ge25519_p1p1 c_cp;
    ge25519_p2 c_pp;
    int i, j;

    cp = *x;
    for (i = 0; i < 32; i++) {
        c = group_ge_neutral;
        for (j = 0; j < 8; j++) {
            group_ge_add(&c, &c, &cp);
            niels_set_p3(&table->v[i*8+j], &c);
        }

        dbl_p1p1(&c_cp, (const ge25519_p2*)&c);
        p1p1_to_p2(&c_pp, &c_cp);
        dbl_p1p1(&c_cp, &c_pp);
        p1p1_to_p2(&c_pp, &c_cp);
        dbl_p1p1(&c_cp, &c_pp);
        p1p1_to_p2(&c_pp, &c_cp);
        dbl_p1p1(&c_cp, &c_pp);
        p1p1_to_p2(&c_pp, &c_cp);
        dbl_p1p1(&c_cp, &c_pp);
        p1p1_to_p3(&cp, &c_cp);
    }
}

static int table_choose_publicinputs(group_niels* p, const group_scalarmult_table* t, int32_t pos, signed char b) {
    if (b == 0)
        return 0;
    if (b < 0)
        niels_neg(p, &t->v[pos*8-b-1]);
    else
        *p = t->v[pos*8+b-1];
    return 1;
}

static void table_choose(group_niels* p, const group_scalarmult_table* t, int32_t pos, signed char b) {
    unsigned char bNegative = negative(b);
    signed char bAbs = (signed char)(b - (signed char)((unsigned char)((-(signed char)bNegative) & b) << 1));
    niels_setone(p);
    int i;
    for (i = 0; i < 8; i++)
        niels_cmov(p, &t->v[pos*8+i], equal(bAbs, (signed char)(i+1)));
    group_niels negP;
    niels_neg(&negP, p);
    niels_cmov(p, &negP, bNegative);
}

void group_ge_scalarmult_table(group_ge *p, const group_scalarmult_table *t, const group_scalar *s)
{
    signed char w[64];
    scalar_window4(w, s);
    *p = group_ge_neutral;

    group_niels np;
    ge25519_p1p1 cp;
    ge25519_p2 pp;
    int i;

    for (i = 1; i < 64; i += 2) {
        table_choose(&np, t, i/2, w[i]);
        niels_mixadd(&cp, p, &np);
        p1p1_to_p3(p, &cp);
    }

    dbl_p1p1(&cp, (const ge25519_p2*)p);
    p1p1_to_p2(&pp, &cp);
    dbl_p1p1(&cp, &pp);
    p1p1_to_p2(&pp, &cp);
    dbl_p1p1(&cp, &pp);
    p1p1_to_p2(&pp, &cp);
    dbl_p1p1(&cp, &pp);
    p1p1_to_p3(p, &cp);

    for (i = 0; i < 64; i += 2) {
        table_choose(&np, t, i/2, w[i]);
        niels_mixadd(&cp, p, &np);
        p1p1_to_p3(p, &cp);
    }
}

void group_ge_scalarmult_table_publicinputs(group_ge *p, const group_scalarmult_table *t, const group_scalar *s)
{
    signed char w[64];
    scalar_window4(w, s);
    *p = group_ge_neutral;

    group_niels np;
    ge25519_p1p1 cp;
    ge25519_p2 pp;
    int i;

    for (i = 1; i < 64; i += 2) {
        if (table_choose_publicinputs(&np, t, i/2, w[i])) {
            niels_mixadd(&cp, p, &np);
            p1p1_to_p3(p, &cp);
        }
    }

    dbl_p1p1(&cp, (const ge25519_p2*)p);
    p1p1_to_p2(&pp, &cp);
    dbl_p1p1(&cp, &pp);
    p1p1_to_p2(&pp, &cp);
    dbl_p1p1(&cp, &pp);
    p1p1_to_p2(&pp, &cp);
    dbl_p1p1(&cp, &pp);
    p1p1_to_p3(p, &cp);

    for (i = 0; i < 64; i += 2) {
        if (table_choose_publicinputs(&np, t, i/2, w[i])) {
            niels_mixadd(&cp, p, &np);
            p1p1_to_p3(p, &cp);
        }
    }
}



/*
void ge_print(const group_ge *a) {
 fe25519_print(&a->x);
 fe25519_print(&a->y);
 fe25519_print(&a->z);
 fe25519_print(&a->t);
}
*/
