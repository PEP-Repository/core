package check

import (
	"crypto/tls"
	"strings"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

func TLSCertificateExpiry(c Context) {
	for _, url := range shared.Conf.CheckTLSCertificateExpiry {
		TLSCertificateExpiryOf(c, url)
	}
}

func TLSCertificateExpiryOf(c Context, url string) {
	conn, err := tls.Dial("tcp", url, &tls.Config{
		InsecureSkipVerify: true,
	})
	if err != nil {
		c.Issuef("%s: error %s", url, err)
		return
	}
	defer conn.Close()

	for _, cert := range conn.ConnectionState().PeerCertificates {
		issuer := strings.Join(cert.Issuer.Organization, ", ")
		daysExpired := int(time.Since(cert.NotAfter).Hours() / 24)

		if daysExpired > 0 {
			c.Issuef("%s: certificate from %s has expired %d days",
				url, issuer, daysExpired)
		} else if daysExpired > -30 {
			c.Issuef("%s: certificate from %s will expire in %d days",
				url, issuer, -daysExpired)
		}
	}
}
