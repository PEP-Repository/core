package http

import (
	"encoding/json"
	"net/http"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
	"pep.cs.ru.nl/pep-watchdog/state"
)

// Handles HTTP GET on /status, used by the pep-watchdog-watchdog.
func statusHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	lastCheck := func() time.Time {
		state.Mux.RLock()
		defer state.Mux.RUnlock()

		return state.Stats.LastCheck
	}()

	json.NewEncoder(w).Encode(
		&struct {
			Deadline time.Time
			Now      time.Time
		}{
			Deadline: lastCheck.Add(shared.Conf.Interval * 2),
			Now:      time.Now(),
		})
}
