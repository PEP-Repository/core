
const fe25519 fe25519_zero = {{0, 0, 0, 0, 0}};
const fe25519 fe25519_one = {{1, 0, 0, 0, 0}};
const fe25519 fe25519_two = {{2, 0, 0, 0, 0}};
const fe25519 fe25519_sqrtm1 = {{1718705420411056, 234908883556509, 2233514472574048, 2117202627021982, 765476049583133}};
const fe25519 fe25519_msqrtm1 = {{ 533094393274173, 2016890930128738, 18285341111199, 134597186663265, 1486323764102114 }};
const fe25519 fe25519_m1 = {{2251799813685228, 2251799813685247, 2251799813685247, 2251799813685247, 2251799813685247}};


/*
 * Ignores top bit of h.
 */
void fe25519_unpack(fe25519 *h,const unsigned char s[32])
{
  h->v[0] = ((crypto_uint64)(s[0]) | ((crypto_uint64)(s[1]) << 8) |
    ((crypto_uint64)(s[2]) << 16) | ((crypto_uint64)(s[3]) << 24) |
    ((crypto_uint64)(s[4]) << 32) | ((crypto_uint64)(s[5]) << 40) |
    ((crypto_uint64)(s[6]&7) << 48));
  h->v[1] = (((crypto_uint64)(s[6]) >> 3) | ((crypto_uint64)(s[7]) << 5) |
    ((crypto_uint64)(s[8]) << 13) | ((crypto_uint64)(s[9]) << 21) |
    ((crypto_uint64)(s[10]) << 29) | ((crypto_uint64)(s[11]) << 37) |
    ((crypto_uint64)(s[12]&63) << 45));
  h->v[2] = (((crypto_uint64)(s[12]) >> 6) | ((crypto_uint64)(s[13]) << 2) |
    ((crypto_uint64)(s[14]) << 10) | ((crypto_uint64)(s[15]) << 18) |
    ((crypto_uint64)(s[16]) << 26) | ((crypto_uint64)(s[17]) << 34) |
    ((crypto_uint64)(s[18]) << 42) | ((crypto_uint64)(s[19]&1) << 50));
  h->v[3] = (((crypto_uint64)(s[19]) >> 1) | ((crypto_uint64)(s[20]) << 7) |
    ((crypto_uint64)(s[21]) << 15) | ((crypto_uint64)(s[22]) << 23) |
    ((crypto_uint64)(s[23]) << 31) | ((crypto_uint64)(s[24]) << 39) |
    ((crypto_uint64)(s[25]&15) << 47));
  h->v[4] = (((crypto_uint64)(s[25]) >> 4) | ((crypto_uint64)(s[26]) << 4) |
    ((crypto_uint64)(s[27]) << 12) | ((crypto_uint64)(s[28]) << 20) |
    ((crypto_uint64)(s[29]) << 28) | ((crypto_uint64)(s[30]) << 36) |
    ((crypto_uint64)(s[31]&127) << 44));
}

void fe25519_set_reduced(fe25519* r, const fe25519* h)
{
  crypto_uint64 h0 = h->v[0];
  crypto_uint64 h1 = h->v[1];
  crypto_uint64 h2 = h->v[2];
  crypto_uint64 h3 = h->v[3];
  crypto_uint64 h4 = h->v[4];

  h1 += h0 >> 51;
  h0 = h0 & 0x7ffffffffffff;
  h2 += h1 >> 51;
  h1 = h1 & 0x7ffffffffffff;
  h3 += h2 >> 51;
  h2 = h2 & 0x7ffffffffffff;
  h4 += h3 >> 51;
  h3 = h3 & 0x7ffffffffffff;
  h0 += (h4 >> 51) * 19;
  h4 = h4 & 0x7ffffffffffff;

  crypto_uint64 c = (h0 + 19) >> 51;
  c = (h1 + c) >> 51;
  c = (h2 + c) >> 51;
  c = (h3 + c) >> 51;
  c = (h4 + c) >> 51;

  h0 += 19 * c;

  h1 += h0 >> 51;
  h0 = h0 & 0x7ffffffffffff;
  h2 += h1 >> 51;
  h1 = h1 & 0x7ffffffffffff;
  h3 += h2 >> 51;
  h2 = h2 & 0x7ffffffffffff;
  h4 += h3 >> 51;
  h3 = h3 & 0x7ffffffffffff;
  h4 = h4 & 0x7ffffffffffff;

  r->v[0] = h0;
  r->v[1] = h1;
  r->v[2] = h2;
  r->v[3] = h3;
  r->v[4] = h4;
}

void fe25519_pack(unsigned char s[32],const fe25519 *h)
{
  crypto_uint64 h0 = h->v[0];
  crypto_uint64 h1 = h->v[1];
  crypto_uint64 h2 = h->v[2];
  crypto_uint64 h3 = h->v[3];
  crypto_uint64 h4 = h->v[4];

  h1 += h0 >> 51;
  h0 = h0 & 0x7ffffffffffff;
  h2 += h1 >> 51;
  h1 = h1 & 0x7ffffffffffff;
  h3 += h2 >> 51;
  h2 = h2 & 0x7ffffffffffff;
  h4 += h3 >> 51;
  h3 = h3 & 0x7ffffffffffff;
  h0 += (h4 >> 51) * 19;
  h4 = h4 & 0x7ffffffffffff;

  crypto_uint64 c = (h0 + 19) >> 51;
  c = (h1 + c) >> 51;
  c = (h2 + c) >> 51;
  c = (h3 + c) >> 51;
  c = (h4 + c) >> 51;

  h0 += 19 * c;

  h1 += h0 >> 51;
  h0 = h0 & 0x7ffffffffffff;
  h2 += h1 >> 51;
  h1 = h1 & 0x7ffffffffffff;
  h3 += h2 >> 51;
  h2 = h2 & 0x7ffffffffffff;
  h4 += h3 >> 51;
  h3 = h3 & 0x7ffffffffffff;
  h4 = h4 & 0x7ffffffffffff;

  s[0] = (unsigned char)(h0 & 0xff);
  s[1] = (unsigned char)((h0 >> 8) & 0xff);
  s[2] = (unsigned char)((h0 >> 16) & 0xff);
  s[3] = (unsigned char)((h0 >> 24) & 0xff);
  s[4] = (unsigned char)((h0 >> 32) & 0xff);
  s[5] = (unsigned char)((h0 >> 40) & 0xff);
  s[6] = (unsigned char)((h0 >> 48));
  s[6] ^= (unsigned char)((h1 << 3) & 0xf8);
  s[7] = (unsigned char)((h1 >> 5) & 0xff);
  s[8] = (unsigned char)((h1 >> 13) & 0xff);
  s[9] = (unsigned char)((h1 >> 21) & 0xff);
  s[10] = (unsigned char)((h1 >> 29) & 0xff);
  s[11] = (unsigned char)((h1 >> 37) & 0xff);
  s[12] = (unsigned char)((h1 >> 45));
  s[12] ^= (unsigned char)((h2 << 6) & 0xc0);
  s[13] = (unsigned char)((h2 >> 2) & 0xff);
  s[14] = (unsigned char)((h2 >> 10) & 0xff);
  s[15] = (unsigned char)((h2 >> 18) & 0xff);
  s[16] = (unsigned char)((h2 >> 26) & 0xff);
  s[17] = (unsigned char)((h2 >> 34) & 0xff);
  s[18] = (unsigned char)((h2 >> 42) & 0xff);
  s[19] = (unsigned char)((h2 >> 50));
  s[19] ^= (unsigned char)((h3 << 1) & 0xfe);
  s[20] = (unsigned char)((h3 >> 7) & 0xff);
  s[21] = (unsigned char)((h3 >> 15) & 0xff);
  s[22] = (unsigned char)((h3 >> 23) & 0xff);
  s[23] = (unsigned char)((h3 >> 31) & 0xff);
  s[24] = (unsigned char)((h3 >> 39) & 0xff);
  s[25] = (unsigned char)((h3 >> 47));
  s[25] ^= (unsigned char)((h4 << 4) & 0xf0);
  s[26] = (unsigned char)((h4 >> 4) & 0xff);
  s[27] = (unsigned char)((h4 >> 12) & 0xff);
  s[28] = (unsigned char)((h4 >> 20) & 0xff);
  s[29] = (unsigned char)((h4 >> 28) & 0xff);
  s[30] = (unsigned char)((h4 >> 36) & 0xff);
  s[31] = (unsigned char)((h4 >> 44));
}

void fe25519_cmov(fe25519 *r, const fe25519 *x, unsigned char b)
{
  crypto_uint64 b2 = (crypto_uint64)(1 - b) - 1;
  r->v[0] ^= b2 & (r->v[0] ^ x->v[0]);
  r->v[1] ^= b2 & (r->v[1] ^ x->v[1]);
  r->v[2] ^= b2 & (r->v[2] ^ x->v[2]);
  r->v[3] ^= b2 & (r->v[3] ^ x->v[3]);
  r->v[4] ^= b2 & (r->v[4] ^ x->v[4]);
}


void fe25519_setone(fe25519 *h)
{
  h->v[0] = 1;
  h->v[1] = 0;
  h->v[2] = 0;
  h->v[3] = 0;
  h->v[4] = 0;
}

void fe25519_setzero(fe25519 *h)
{
  h->v[0] = 0;
  h->v[1] = 0;
  h->v[2] = 0;
  h->v[3] = 0;
  h->v[4] = 0;
}


void fe25519_neg(fe25519 *h, const fe25519 *g)
{
  crypto_uint64 t0 = g->v[0];
  crypto_uint64 t1 = g->v[1];
  crypto_uint64 t2 = g->v[2];
  crypto_uint64 t3 = g->v[3];
  crypto_uint64 t4 = g->v[4];
  
  t1 += t0 >> 51;
  t0 = t0 & 0x7ffffffffffff;
  t2 += t1 >> 51;
  t1 = t1 & 0x7ffffffffffff;
  t3 += t2 >> 51;
  t2 = t2 & 0x7ffffffffffff;
  t4 += t3 >> 51;
  t3 = t3 & 0x7ffffffffffff;
  t0 += (t4 >> 51) * 19;
  t4 = t4 & 0x7ffffffffffff;

  h->v[0] = 0xfffffffffffda - t0;
  h->v[1] = 0xffffffffffffe - t1;
  h->v[2] = 0xffffffffffffe - t2;
  h->v[3] = 0xffffffffffffe - t3;
  h->v[4] = 0xffffffffffffe - t4;
}

void fe25519_add(fe25519 *h,const fe25519 *f,const fe25519 *g)
{
  crypto_uint64 f0 = f->v[0];
  crypto_uint64 f1 = f->v[1];
  crypto_uint64 f2 = f->v[2];
  crypto_uint64 f3 = f->v[3];
  crypto_uint64 f4 = f->v[4];
  crypto_uint64 g0 = g->v[0];
  crypto_uint64 g1 = g->v[1];
  crypto_uint64 g2 = g->v[2];
  crypto_uint64 g3 = g->v[3];
  crypto_uint64 g4 = g->v[4];
  crypto_uint64 h0 = f0 + g0;
  crypto_uint64 h1 = f1 + g1;
  crypto_uint64 h2 = f2 + g2;
  crypto_uint64 h3 = f3 + g3;
  crypto_uint64 h4 = f4 + g4;
  h->v[0] = h0;
  h->v[1] = h1;
  h->v[2] = h2;
  h->v[3] = h3;
  h->v[4] = h4;
}

void fe25519_sub(fe25519 *h,const fe25519 *f,const fe25519 *g)
{
  crypto_uint64 t0 = g->v[0];
  crypto_uint64 t1 = g->v[1];
  crypto_uint64 t2 = g->v[2];
  crypto_uint64 t3 = g->v[3];
  crypto_uint64 t4 = g->v[4];
  crypto_uint64 f0 = f->v[0];
  crypto_uint64 f1 = f->v[1];
  crypto_uint64 f2 = f->v[2];
  crypto_uint64 f3 = f->v[3];
  crypto_uint64 f4 = f->v[4];
  
  t1 += t0 >> 51;
  t0 = t0 & 0x7ffffffffffff;
  t2 += t1 >> 51;
  t1 = t1 & 0x7ffffffffffff;
  t3 += t2 >> 51;
  t2 = t2 & 0x7ffffffffffff;
  t4 += t3 >> 51;
  t3 = t3 & 0x7ffffffffffff;
  t0 += (t4 >> 51) * 19;
  t4 = t4 & 0x7ffffffffffff;

  h->v[0] = (f0 + 0xfffffffffffda) - t0;
  h->v[1] = (f1 + 0xffffffffffffe) - t1;
  h->v[2] = (f2 + 0xffffffffffffe) - t2;
  h->v[3] = (f3 + 0xffffffffffffe) - t3;
  h->v[4] = (f4 + 0xffffffffffffe) - t4;
}

void fe25519_mul(fe25519 *h,const fe25519 *f,const fe25519 *g)
{
  crypto_uint64 f0 = f->v[0];
  crypto_uint64 f1 = f->v[1];
  crypto_uint64 f2 = f->v[2];
  crypto_uint64 f3 = f->v[3];
  crypto_uint64 f4 = f->v[4];
  crypto_uint64 g0 = g->v[0];
  crypto_uint64 g1 = g->v[1];
  crypto_uint64 g2 = g->v[2];
  crypto_uint64 g3 = g->v[3];
  crypto_uint64 g4 = g->v[4];
  crypto_uint64 g1_19 = g1 * 19;
  crypto_uint64 g2_19 = g2 * 19;
  crypto_uint64 g3_19 = g3 * 19;
  crypto_uint64 g4_19 = g4 * 19;

  uint128_t c0 = 
      (uint128_t)(g0) * (uint128_t)(f0) +
      (uint128_t)(g1_19) * (uint128_t)(f4) +
      (uint128_t)(g2_19) * (uint128_t)(f3) +
      (uint128_t)(g3_19) * (uint128_t)(f2) +
      (uint128_t)(g4_19) * (uint128_t)(f1);
  uint128_t c1 = 
      (uint128_t)(g0) * (uint128_t)(f1) +
      (uint128_t)(g1) * (uint128_t)(f0) +
      (uint128_t)(g2_19) * (uint128_t)(f4) +
      (uint128_t)(g3_19) * (uint128_t)(f3) +
      (uint128_t)(g4_19) * (uint128_t)(f2);
  uint128_t c2 = 
      (uint128_t)(g0) * (uint128_t)(f2) +
      (uint128_t)(g1) * (uint128_t)(f1) +
      (uint128_t)(g2) * (uint128_t)(f0) +
      (uint128_t)(g3_19) * (uint128_t)(f4) +
      (uint128_t)(g4_19) * (uint128_t)(f3);
  uint128_t c3 = 
      (uint128_t)(g0) * (uint128_t)(f3) +
      (uint128_t)(g1) * (uint128_t)(f2) +
      (uint128_t)(g2) * (uint128_t)(f1) +
      (uint128_t)(g3) * (uint128_t)(f0) +
      (uint128_t)(g4_19) * (uint128_t)(f4);
  uint128_t c4 = 
      (uint128_t)(g0) * (uint128_t)(f4) +
      (uint128_t)(g1) * (uint128_t)(f3) +
      (uint128_t)(g2) * (uint128_t)(f2) +
      (uint128_t)(g3) * (uint128_t)(f1) +
      (uint128_t)(g4) * (uint128_t)(f0);

  c1 += (crypto_uint64)(c0 >> 51);
  crypto_uint64 h0 = (crypto_uint64)(c0) & 0x7ffffffffffff;
  c2 += (crypto_uint64)(c1 >> 51);
  crypto_uint64 h1 = (crypto_uint64)(c1) & 0x7ffffffffffff;
  c3 += (crypto_uint64)(c2 >> 51);
  crypto_uint64 h2 = (crypto_uint64)(c2) & 0x7ffffffffffff;
  c4 += (crypto_uint64)(c3 >> 51);
  crypto_uint64 h3 = (crypto_uint64)(c3) & 0x7ffffffffffff;
  crypto_uint64 carry = (crypto_uint64)(c4 >> 51);
  crypto_uint64 h4 = (crypto_uint64)(c4) & 0x7ffffffffffff;

  h0 += carry * 19;
  h1 += h0 >> 51;
  h0 &= 0x7ffffffffffff;

  h->v[0] = h0;
  h->v[1] = h1;
  h->v[2] = h2;
  h->v[3] = h3;
  h->v[4] = h4;
}

void fe25519_square(fe25519 *h,const fe25519 *f)
{
  crypto_uint64 f0 = f->v[0];
  crypto_uint64 f1 = f->v[1];
  crypto_uint64 f2 = f->v[2];
  crypto_uint64 f3 = f->v[3];
  crypto_uint64 f4 = f->v[4];
  crypto_uint64 f3_19 = f3 * 19;
  crypto_uint64 f4_19 = f4 * 19;

  uint128_t c0 = 
      (uint128_t)(f0) * (uint128_t)(f0) +
      2 * (uint128_t)(f1) * (uint128_t)(f4_19) +
      2 * (uint128_t)(f3_19) * (uint128_t)(f2);
  uint128_t c1 = 
      2 * (uint128_t)(f1) * (uint128_t)(f0) +
      2 * (uint128_t)(f2) * (uint128_t)(f4_19) +
      (uint128_t)(f3_19) * (uint128_t)(f3);
  uint128_t c2 = 
      2 * (uint128_t)(f0) * (uint128_t)(f2) +
      (uint128_t)(f1) * (uint128_t)(f1) +
      2 * (uint128_t)(f4_19) * (uint128_t)(f3);
  uint128_t c3 = 
      2 * (uint128_t)(f0) * (uint128_t)(f3) +
      2 * (uint128_t)(f2) * (uint128_t)(f1) +
      (uint128_t)(f4_19) * (uint128_t)(f4);
  uint128_t c4 = 
      2 * (uint128_t)(f0) * (uint128_t)(f4) +
      (uint128_t)(f2) * (uint128_t)(f2) +
      2 * (uint128_t)(f3) * (uint128_t)(f1);

  c1 += (crypto_uint64)(c0 >> 51);
  crypto_uint64 h0 = (crypto_uint64)(c0) & 0x7ffffffffffff;
  c2 += (crypto_uint64)(c1 >> 51);
  crypto_uint64 h1 = (crypto_uint64)(c1) & 0x7ffffffffffff;
  c3 += (crypto_uint64)(c2 >> 51);
  crypto_uint64 h2 = (crypto_uint64)(c2) & 0x7ffffffffffff;
  c4 += (crypto_uint64)(c3 >> 51);
  crypto_uint64 h3 = (crypto_uint64)(c3) & 0x7ffffffffffff;
  crypto_uint64 carry = (crypto_uint64)(c4 >> 51);
  crypto_uint64 h4 = (crypto_uint64)(c4) & 0x7ffffffffffff;

  h0 += carry * 19;
  h1 += h0 >> 51;
  h0 &= 0x7ffffffffffff;

  h->v[0] = h0;
  h->v[1] = h1;
  h->v[2] = h2;
  h->v[3] = h3;
  h->v[4] = h4;
}

void fe25519_square_double(fe25519 *h,const fe25519 *f)
{
  // Fusing square and double doesn't seem to make a big difference
  // in this case, so we don't.
  fe25519_square(h, f);
  fe25519_add(h, h, h);
}

