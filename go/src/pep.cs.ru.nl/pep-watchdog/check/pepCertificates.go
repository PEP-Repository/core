package check

// Check expiry of certificates on servers and their clock drift.

import (
	"crypto/x509"
	"fmt"
	"strings"
	"time"

	"generated.pep.cs.ru.nl/gopep/pep_proto"
	pep "pep.cs.ru.nl/gopep"
	"pep.cs.ru.nl/gopep/pep_protocol"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

func PEPCertificateExpiry(c Context) {
	PEPRootCertificate(c)

	if !shared.Pep.Enrolled() {
		return
	}

	for name, conn := range shared.Pep.Servers() {
		PEPCertificateExpiryOf(c, name, conn)
	}
}

func PEPRootCertificate(c Context) {
	cert := shared.Pep.RootCaCert()
	daysExpired := int(time.Since(cert.NotAfter).Hours() / 24)

	if daysExpired > 0 {
		c.Issuef("Root certificate has expired %d days", daysExpired)
	} else if daysExpired > -90 {
		c.Issuef("Root certificate will expire in %d days",
			-daysExpired)
	}
}

func PEPCertificateExpiryOf(
	c Context,
	name string,
	conn *pep_protocol.Conn) {

	for _, cert := range conn.PeerCertificates() {
		issuer := strings.Join(cert.Issuer.Organization, ", ")
		daysExpired := int(time.Since(cert.NotAfter).Hours() / 24)

		if daysExpired > 0 {
			c.Issuef("%s: TLS certificate from %s has expired %d days",
				name, issuer, daysExpired)
		} else if daysExpired > -30 {
			c.Issuef("%s: TLS certificate from %s will expire in %d days",
				name, issuer, -daysExpired)
		}
	}

	ts, certs, err := GetTimeAndPEPCertificates(conn)
	if err != nil {
		c.Issuef("%s: failed to retrieve server time or PEP certificates: %s", name, err)
		return
	}

	if ts == nil {
		c.Issuef("%s: does not report time in PingResponse", name)
	} else {
		drift := time.Since(*ts).Seconds()
		if drift > 10 {
			c.Issuef("%s: clock has drifted by %.0f seconds", name, drift)
		}
	}

	for _, cert := range certs {
		daysExpired := int(time.Since(cert.NotAfter).Hours() / 24)

		if daysExpired > 0 {
			c.Issuef("%s: PEP certificate has expired %d days", name, daysExpired)
		} else if daysExpired > -30 {
			c.Issuef("%s: PEP certificate will expire in %d days",
				name, -daysExpired)
		}
	}
}

func GetTimeAndPEPCertificates(c *pep_protocol.Conn) (ts *time.Time, certs []*x509.Certificate, err error) {
	rawResp, err := c.Request(&pep_proto.PingRequest{})
	if err != nil {
		return
	}
	var chain pep.SigningCertificateChain
	var pingResp *pep_proto.PingResponse
	gotCerts := false
	switch resp := rawResp.(type) {
	case *pep_proto.PingResponse:
		pingResp = resp
	case *pep_proto.SignedPingResponse:
		chain.SetProto(resp.Signature.CertificateChain)
		gotCerts = true

		var almostPingResp pep_protocol.Message
		almostPingResp, err = pep_protocol.OpenSigned(resp)
		if err != nil {
			return
		}
		var ok bool
		pingResp, ok = almostPingResp.(*pep_proto.PingResponse)
		if !ok {
			err = fmt.Errorf("Ping(): wrong wrapped response message")
			return
		}
	default:
		err = fmt.Errorf("Ping(): wrong response type")
		return
	}
	if gotCerts {
		certs, err = chain.GetCertificates()
	}
	tsm := pingResp.Timestamp.EpochMillis
	tsv := time.Unix(tsm/1000, (tsm%1000)*1000000)
	ts = &tsv
	return
}
