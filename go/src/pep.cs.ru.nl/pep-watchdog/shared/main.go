package shared

import (
	"fmt"
	"log"
	"time"

	pep "pep.cs.ru.nl/gopep"

	"github.com/bwesterb/bolthold"
)

// State shared between all parts of the watchdog, set on start-up
var (
	Pep      *pep.Context
	Db       *bolthold.Store
	Stressor StressorType
)

func Setup() {
	ParseArgs()
	LoadConf()
	LoadTokenFile()

}

// Stressor made to test the acceptance environment's new S3 code,
// see StressorConf in main.go for more information.
type StressorType interface {
	// when the stressor ran and failed,
	// returns the error that caused the failure,
	// and the time of failure
	State() StressorState
}

type StressorState struct {
	Err     error
	ErrTime time.Time

	TestRunning bool
	TestStarted time.Time
}

func NewPepContext() (context *pep.Context) {
	context, err := pep.NewContext(
		Conf.ConstellationFile,
		Conf.SecretsFile,
		pep.ContextOptions{
			Timeout: 30 * time.Second,
			Patient: true,
		})
	if err != nil {
		log.Fatalf("Could not create context: %v", err)
	}
	return
}

// Ensure the context is enrolled for the given duration
func EnsureEnrolled(interval time.Duration) error {

	t := time.Now().Add(interval)

	if Pep.EnrolledUntil(t) {
		return nil
	}

	err := Pep.EnrollUser(TokenFile.OAuthToken)
	if err != nil {
		return fmt.Errorf("Failed to enroll: %v", err)
	}

	if !Pep.EnrolledUntil(t) {
		log.Fatalf("Could not enroll for %v.", interval)
	}

	err = Pep.SaveSecrets()
	if err != nil {
		log.Fatalf("SaveSecrets: %v", err)
	}

	return nil
}
