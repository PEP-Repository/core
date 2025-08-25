package pep

// Contains the definition of Triple, an El-Gamal triple, which is the main
// cryptographic protagonist of the PEP system:
//
//    a polymorphic pseudonym is a Triple,
//    an PEP-encrypted symmetric key is a Triple.

import (
	"crypto/sha256"
	"errors"
	"fmt"

	"github.com/bwesterb/go-ristretto"

	"generated.pep.cs.ru.nl/gopep/pep_proto"
)

// An El-Gamal triple (b, c, y), which is used in PEP as a polymorphic
// pseudonym or a rekey-able ciphertext.
type Triple struct {
	// Blinding component
	b ristretto.Point

	// Ciphertext component
	c ristretto.Point

	// Public key
	y ristretto.Point
}

func (t Triple) B() ristretto.Point { return t.b }
func (t Triple) C() ristretto.Point { return t.c }
func (t Triple) Y() ristretto.Point { return t.y }

func NewTriple(b, c, y ristretto.Point) *Triple {
	return &Triple{b, c, y}
}

// Decrypt the triple, given the secret key sk.
func (t *Triple) Decrypt(sk *ristretto.Scalar) *ristretto.Point {
	var ret ristretto.Point
	return ret.ScalarMult(&t.b, sk).Sub(&t.c, &ret)
}

// Set t to the p encrypted for y.
func (t *Triple) Encrypt(p *ristretto.Point, y *ristretto.Point) error {
	allzeroes := true
	for _, v := range y.Bytes() {
		if v != 0 {
			allzeroes = false
			break
		}
	}
	if allzeroes {
		return errors.New("Cannot encrypt using a zero public key")
	}

	var r ristretto.Scalar
	var ry ristretto.Point
	r.Rand()
	t.b.ScalarMultBase(&r)
	ry.ScalarMult(y, &r)
	t.c.Add(p, &ry)
	t.y.Set(y)
	return nil // No error occurred
}

// Rerandomizes the triple
func (t *Triple) Rerandomize() {
	var s ristretto.Scalar
	var sB, sy ristretto.Point
	s.Rand()
	sB.ScalarMultBase(&s)
	sy.ScalarMult(&t.y, &s)
	t.b.Add(&t.b, &sB)
	t.c.Add(&t.c, &sy)
}

// Rekey a triple by a translation s
func (t *Triple) Rekey(s *ristretto.Scalar) {
	var sInv ristretto.Scalar
	sInv.Inverse(s)
	t.y.ScalarMult(&t.y, s)
	t.b.ScalarMult(&t.b, &sInv)
}

// Reshuffle an triple
func (t *Triple) Reshuffle(s *ristretto.Scalar) {
	t.b.ScalarMult(&t.b, s)
	t.c.ScalarMult(&t.c, s)
}

// Checks whether a Triple equals another
func (t *Triple) Equals(other *Triple) bool {
	return t.b.Equals(&other.b) && t.c.Equals(&other.c) && t.y.Equals(&other.y)
}

// Derives a symmetric key from a curvepoint
func SymmetricKeyFromPoint(p *ristretto.Point) []byte {
	h := sha256.Sum256(p.Bytes())
	return h[:]
}

func (t *Triple) proto() *pep_proto.ElgamalEncryption {
	var ret pep_proto.ElgamalEncryption
	ret.B = &pep_proto.CurvePoint{CurvePoint: t.b.Bytes()}
	ret.C = &pep_proto.CurvePoint{CurvePoint: t.c.Bytes()}
	ret.Y = &pep_proto.CurvePoint{CurvePoint: t.y.Bytes()}
	return &ret
}

func (t *Triple) setProto(v *pep_proto.ElgamalEncryption) {
	t.b.SetBytes((*[32]byte)(v.B.CurvePoint))
	t.c.SetBytes((*[32]byte)(v.C.CurvePoint))
	t.y.SetBytes((*[32]byte)(v.Y.CurvePoint))
}

func (t Triple) String() string {
	return fmt.Sprintf("EGT{%s %s %s}", t.b, t.c, t.y)
}
