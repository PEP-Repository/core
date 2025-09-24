package check

import (
	"github.com/golang/protobuf/proto"

	"pep.cs.ru.nl/gopep/pep_protocol"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

func PEPVersions(c Context) {
	if !shared.Pep.Enrolled() {
		return
	}

	myVersion, err := pep_protocol.GetCurrentBuildVersionResponse()
	if err != nil {
		c.Issuef("Failed to determine own version to compare to server versions: %v",
			err)
		return
	}

	for name, conn := range shared.Pep.Servers() {
		version, err := conn.Version()
		if err != nil {
			c.Issuef("%s: failed to retrieve version: %v",
				name, err)
			continue
		}
		if !proto.Equal(myVersion, version) {
			c.Issuef("Watchdog version (%v) does not match "+
				"the %s version (%v)",
				myVersion, name, version)
		}
	}
}
