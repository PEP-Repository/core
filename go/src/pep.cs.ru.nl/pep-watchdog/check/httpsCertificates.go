package check

import (
	"net/http"
	"strings"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

func HTTPSCertificateExpiry(c Context) {
	for _, url := range shared.Conf.CheckHTTPSCertificateExpiry {
		HTTPSCertificateExpiryOf(c, url)
	}
}

func HTTPSCertificateExpiryOf(c Context, url string) {
	httpClient := http.Client{
		Timeout: time.Duration(30 * time.Second),
	}
	req, err := http.NewRequestWithContext(c, "HEAD", url, nil)
	if err != nil {
		c.Issuef("%s: error %s", url, err)
		return
	}
	resp, err := httpClient.Do(req)
	if err != nil {
		c.Issuef("%s: error %s", url, err)
		return
	}
	defer resp.Body.Close()
	if resp.TLS == nil {
		c.Issuef("%s: no TLS enabled", url)
		return
	}

	for _, cert := range resp.TLS.PeerCertificates {
		issuer := strings.Join(cert.Issuer.Organization, ", ")
		daysExpired := int(time.Since(cert.NotAfter).Hours() / 24)
		if daysExpired > 0 {
			c.Issuef("%s: certificate from %s has expired %d days",
				url, issuer, daysExpired)
		} else if daysExpired > -21 {
			c.Issuef("%s: certificate from %s will expire "+
				"in %d days",
				url, issuer, -daysExpired)
		}
	}
}
