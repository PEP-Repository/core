package state

import (
	"log"
	"time"

	"github.com/bwesterb/bolthold"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

type PersistentIssueRecord struct {
	At      time.Time
	Message string
}

func GetPersistentIssues() (persistentIssues []PersistentIssueRecord) {
	err := shared.Db.Find(&persistentIssues, nil)
	if err != nil {
		log.Fatalf("failed to load persistent issues: %v", err)
	}
	return
}

func RecordPersistentIssue(msg string) {
	err := shared.Db.Insert(bolthold.NextSequence(),
		&PersistentIssueRecord{
			At:      time.Now(),
			Message: msg,
		})
	if err != nil {
		log.Fatalf("Failed to insert persistent issue into database:"+
			" %v", err)
	}
}

func ClearPersistentIssues() {
	err := shared.Db.DeleteMatching(&PersistentIssueRecord{}, nil)
	if err != nil {
		log.Fatalf("Failed to clear persistent issues: %v", err)
	}
}
