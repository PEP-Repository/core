package check

import (
	"crypto/rand"
	"log"
	"time"

	"github.com/bwesterb/bolthold"

	"pep.cs.ru.nl/pep-watchdog/canaries"
	"pep.cs.ru.nl/pep-watchdog/shared"
)

func CanaryFiles(c Context) {
	if !shared.Pep.Enrolled() {
		return
	}

	records, unknownCanaries, missingCanaries, err := canaries.Get()
	if err != nil {
		c.Issuef("checkCanaryFiles:  %v", err)
		return
	}
	for _, r := range unknownCanaries {
		c.Issuef("Unknown canary file with value %x", r.Value)
	}
	for _, r := range missingCanaries {
		c.Issuef("Canary file with value %x is missing", r.Value)
	}

	// Check if we need to create a new canary file
	currentWindow := time.Now().Truncate(shared.Conf.CanaryFile.Interval)
	ok := false
	for _, r := range records {
		if r.Created.Truncate(shared.Conf.CanaryFile.Interval) ==
			currentWindow {

			ok = true
			break
		}
	}
	if ok {
		return
	}
	pp, err := shared.Pep.DerivePolymorphicPseudonym(
		shared.Conf.CanaryFile.Identity)
	if err != nil {
		c.Issuef("Could not create canary file: %v", err)
		return
	}
	w, err := shared.Pep.CreateFile(pp, shared.Conf.CanaryFile.Tag)
	if err != nil {
		c.Issuef("Could not create canary file: %v", err)
		return
	}

	var contents [32]byte
	rand.Read(contents[:])
	_, err = w.Write(contents[:])
	if err != nil {
		c.Issuef("Could not write to new canary file: %v", err)
		return
	}

	err = w.Close()
	if err != nil {
		c.Issuef("Could not close canary file: %v", err)
		return
	}

	identifier := <-w.IdentifierC

	// Store canary file in database
	record := canaries.CanaryFileRecord{
		Created:     time.Now(),
		Identifier2: identifier,
		Value:       contents[:],
	}

	err = shared.Db.Insert(bolthold.NextSequence(), &record)
	if err != nil {
		c.Issuef("Failed to store record of new canary file: %v", err)
		return
	}

	log.Printf("Created new canary file with ID %x and value %x",
		identifier, contents[:])
}
