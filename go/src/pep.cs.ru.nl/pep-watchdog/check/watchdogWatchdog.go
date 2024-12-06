package check

import (
	"encoding/json"
	"io/ioutil"
	"net/http"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

func WatchdogWatchdog(c Context) {
	if shared.Conf.WatchdogWatchdogUrl == "" {
		return
	}
	httpClient := http.Client{
		Timeout: time.Duration(30 * time.Second),
	}
	req, err := http.NewRequestWithContext(c, "GET", shared.Conf.WatchdogWatchdogUrl, nil)
	if err != nil {
		c.Issuef("Error creating request to watchdog-watchdog: %s", err)
		return
	}
	resp, err := httpClient.Do(req)
	if err != nil {
		c.Issuef("Couldn't reach watchdog-watchdog: %s", err)
		return
	}

	buf, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		c.Issuef("Couldn't reach watchdog-watchdog: %v", err)
		return
	}

	var status struct {
		Deadline time.Time
		Now      time.Time
	}
	err = json.Unmarshal(buf, &status)
	if err != nil {
		c.Issuef("Couldn't parse watchdog-watchdog response: %v", err)
		return
	}

	if status.Now.After(status.Deadline) {
		c.Issuef("Watchdog-watchdog is running, " +
			"but didn't finish a check in time")
		return
	}
}
