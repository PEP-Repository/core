package http

import (
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"log"
	"net/http"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

func ListenAndServe() {
	setupRootHandler()

	http.HandleFunc("/", rootHandler)
	http.HandleFunc("/status", statusHandler)
	http.HandleFunc("/badge.png", badgeHandler)
	http.Handle("/metrics", promhttp.Handler())
	http.HandleFunc("/metrics/", proxyMetricsHandler)

	log.Printf("Listening on %s ...", shared.Conf.BindAddr)
	log.Fatal(http.ListenAndServe(shared.Conf.BindAddr, nil))

}
