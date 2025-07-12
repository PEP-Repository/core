package check

import (
	"time"

	pep "pep.cs.ru.nl/gopep"
	"pep.cs.ru.nl/gopep/pep_protocol"

	"pep.cs.ru.nl/pep-watchdog/consts"
	"pep.cs.ru.nl/pep-watchdog/shared"
)

func TokenExpiry(c Context) {
	tokenData, _, err := pep.ParseOAuthToken(shared.TokenFile.OAuthToken)
	if err != nil {
		c.Issuef("Parsing token failed: %v", err)
	}
	daysExpired := int(time.Since(time.Unix(
		tokenData.ExpiresAt, 0)).Hours() / 24)

	if daysExpired > 0 {
		c.Issuef("OAuthToken for watchdog has expired %d days",
			daysExpired)
	} else if daysExpired > -90 {
		c.Issuef("OAuthToken for watchdog will expire in %d days",
			-daysExpired)
	}
}

func Enrollment(c Context) {
	// Enroll, but discard the secrets obtained
	_, err := shared.Pep.EnrollUserReturnSecrets(
		shared.TokenFile.OAuthToken)
	if err != nil {
		c.Issuef("Enrollment failed: %v", err)
	}
}

func PEPServers(c Context) {
	for name, conn := range shared.Pep.Servers() {
		PEPServer(c, name, conn)
	}
}

func PEPServer(c Context, name string, conn *pep_protocol.Conn) {
	if err := conn.Ping(); err != nil {
		c.Issuef("Can't reach %s: %v", name, err.Error())
	}
}

func Stressor(c Context) {
	sstate := shared.Stressor.State()

	if sstate.Err != nil {
		c.Issuef("Stressor stopped at %s: %v. "+
			"(Restart the watchdog to reset the stressor.)",
			sstate.ErrTime.Format(consts.TimeFormat), sstate.Err)
	}

	// check whether the stressor is exceeding its time limit
	if sstate.TestRunning &&
		(time.Now().Sub(sstate.TestStarted) >
			shared.Conf.Stressor.TimeLimit) {

		c.Issuef("The stressor, running since %s, "+
			"should have finished before %s.",
			sstate.TestStarted.Format(consts.TimeFormat),
			(sstate.TestStarted.Add(
				shared.Conf.Stressor.TimeLimit).Format(consts.TimeFormat)))
	}
}
