package http

import (
	pep "pep.cs.ru.nl/gopep"
	"pep.cs.ru.nl/gopep/pep_protocol"

	"fmt"
	"net/http"
	"strings"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

// Handles HTTP GET on /metrics/<server> which retrieves and returns
// the metrics collected by the given server via the PEP protocol.
func proxyMetricsHandler(w http.ResponseWriter, r *http.Request) {
	name := strings.TrimPrefix(r.URL.Path, "/metrics/")
	var conn *pep_protocol.Conn
	switch name {
	case "transcryptor":
		conn = shared.Pep.TS().Conn()
	case "accessmanager":
		conn = shared.Pep.AM().Conn()
	case "storagefacility":
		conn = shared.Pep.SF().Conn()
	case "keyserver":
		conn = shared.Pep.KS().Conn()
	case "authserver":
		conn = shared.Pep.AS().Conn()
	default:
		http.Error(w, fmt.Sprintf("No such server: %v", name), 404)
		return
	}

	err := shared.EnsureEnrolled(5 * time.Minute)
	if err != nil {
		http.Error(w, fmt.Sprintf("%v", err), 500)
		return
	}

	metrics, err := pep.RetrieveMetrics(shared.Pep.SigningKey(), conn)
	if err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve metrics: %v", err), 500)
		return
	}

	w.Write(metrics)
}
