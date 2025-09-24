package pep

// Contains CertifiedSigningPrivateKey, which is used to sign messages passed
// over the PEP protocol.

import (
	"generated.pep.cs.ru.nl/gopep/pep_proto"
	"pep.cs.ru.nl/gopep/pep_protocol"

	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha512"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/binary"
	"encoding/pem"
	"fmt"
	"time"
)

// Private key used to sign protocol messages
type SigningPrivateKey rsa.PrivateKey

// Public key corresponding to a SigningPrivateKey
type SigningPublicKey rsa.PublicKey

// Chain to cerify a SigningPublicKey
type SigningCertificateChain struct {
	data   [][]byte
	expiry time.Time
}

// A signing private key together with a certificate chain for its public key
type CertifiedSigningPrivateKey struct {
	PrivateKey       SigningPrivateKey
	CertificateChain SigningCertificateChain
}

// Signs a pep_protocol.Message
func (p *CertifiedSigningPrivateKey) SignMessage(
	msg pep_protocol.Message) (pep_protocol.Message, error) {
	return pep_protocol.SignMessage(msg, p.MessageSigner(false))
}

// Returns a MessageSigningFunc that can be used to create signatures.
func (p *CertifiedSigningPrivateKey) MessageSigner(isLogCopy bool) pep_protocol.MessageSigningFunc {
	return func(data []byte) (*pep_proto.Signature, error) {
		scheme := pep_proto.SignatureScheme_SIGNATURE_SCHEME_V4
		h := sha512.New()
		ts := time.Now().UnixNano() / 1000000
		packedScheme := make([]byte, 4)
		binary.BigEndian.PutUint32(packedScheme, uint32(scheme))
		packedTs := make([]byte, 8)
		binary.BigEndian.PutUint64(packedTs, uint64(ts))
		h.Write(packedScheme)
		h.Write(packedTs)
		if isLogCopy {
			h.Write([]byte{1})
		} else {
			h.Write([]byte{0})
		}
		h.Write(data)
		sigBytes, err := rsa.SignPKCS1v15(
			rand.Reader,
			p.PrivateKey.rsa(),
			crypto.SHA256,
			h.Sum([]byte{})[:32])
		if err != nil {
			return nil, err
		}
		return &pep_proto.Signature{
			CertificateChain: p.CertificateChain.proto(),
			Signature:        sigBytes,
			Scheme:           scheme,
			Timestamp:        &pep_proto.Timestamp{EpochMillis: ts},
			IsLogCopy:        isLogCopy,
		}, nil
	}
}

// Set sk to a newly generated keypair.
func (sk *SigningPrivateKey) Generate() error {
	//log.Printf("Generating RSA keypair ...")
	rsaSk, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		return err
	}
	*sk = *(*SigningPrivateKey)(rsaSk)
	return nil
}

// Create certificate signing request from SigningPrivateKey
func (sk *SigningPrivateKey) CreateCSR(user, group string) ([]byte, error) {
	template := x509.CertificateRequest{
		Subject: pkix.Name{
			CommonName:         user,
			OrganizationalUnit: []string{group},
		},
		SignatureAlgorithm: x509.SHA256WithRSA,
	}
	return x509.CreateCertificateRequest(rand.Reader, &template, sk.rsa())
}

func (c *SigningCertificateChain) MarshalText() ([]byte, error) {
	ret := []byte{}
	for _, crt := range c.data {
		ret = append(ret, pem.EncodeToMemory(&pem.Block{
			Type:  "CERTIFICATE",
			Bytes: crt})...)
	}
	return ret, nil
}

func (c *SigningCertificateChain) UnmarshalText(text []byte) error {
	new_data := [][]byte{}
	var block *pem.Block
	for len(text) != 0 {
		block, text = pem.Decode(text)
		if block == nil {
			return fmt.Errorf("%v is not a PEM block", text)
		}
		// XXX should we check the Type?
		new_data = append(new_data, block.Bytes)
	}
	c.data = new_data
	return c.parse()
}

func (c SigningCertificateChain) String() string {
	text, _ := c.MarshalText()
	return string(text)
}

func (c *SigningCertificateChain) SetProto(p *pep_proto.X509CertificateChain) {
	new_data := make([][]byte, len(p.Certificate))
	for i, crt := range p.Certificate {
		new_data[i] = crt.Data
	}
	c.data = new_data
	c.parse()
}

func (c *SigningCertificateChain) GetCertificates() ([]*x509.Certificate, error) {
	var ret []*x509.Certificate
	for _, buf := range c.data {
		cert, err := x509.ParseCertificate(buf)
		if err != nil {
			return nil, err
		}
		ret = append(ret, cert)
	}
	return ret, nil
}

// Extract expiry date for this certificate chain
func (c *SigningCertificateChain) parse() error {
	certs, err := c.GetCertificates()
	if err != nil {
		return err
	}
	for i, cert := range certs {
		if i == 0 || c.expiry.After(cert.NotAfter) {
			c.expiry = cert.NotAfter
		}
	}
	return nil
}

func (c *SigningCertificateChain) ExpiredOn(t time.Time) bool {
	return t.After(c.expiry)
}

func (c *SigningCertificateChain) proto() *pep_proto.X509CertificateChain {
	ret := pep_proto.X509CertificateChain{
		Certificate: make([]*pep_proto.X509Certificate, len(c.data)),
	}
	for i, crt := range c.data {
		ret.Certificate[i] = &pep_proto.X509Certificate{
			Data: crt,
		}
	}
	return &ret
}

func (sk *SigningPrivateKey) rsa() *rsa.PrivateKey {
	return (*rsa.PrivateKey)(sk)
}
