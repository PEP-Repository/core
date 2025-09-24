package main

// TODO: check timeout of stressor

import (
	"bytes"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"github.com/hashicorp/go-multierror"
	"io"
	"log"
	pep "pep.cs.ru.nl/gopep"
	"sync/atomic"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

type stressorImp struct {
	state atomic.Value
}

func (stressor *stressorImp) State() shared.StressorState {
	return stressor.state.Load().(shared.StressorState)
}

// reads its options from the global shared.Conf.Stressor
func NewStressor() shared.StressorType {
	var stressor = &stressorImp{}
	stressor.state.Store(shared.StressorState{})

	if shared.Args.Command == shared.OneShot {
		// do not wait
		stressor.do()
	} else {
		waitTime := shared.Conf.Stressor.StartAfter

		if shared.Args.InstantStressor {
			waitTime = 0 * time.Second
		}

		time.AfterFunc(waitTime, stressor.do)
	}

	return stressor
}

func (stressor *stressorImp) do() {
	stressor.state.Store(shared.StressorState{
		TestRunning: true,
		TestStarted: time.Now(),
	})
	log.Printf("Starting stressor ... ")
	if err := stressor.doOne(); err != nil {
		stressor.state.Store(shared.StressorState{
			Err:     err,
			ErrTime: time.Now(),
		})
		log.Printf("Stressor: Error: %v", err)
		return
	}
	log.Printf("Stressor done")
	stressor.state.Store(shared.StressorState{})
	if shared.Args.Command != shared.OneShot {
		time.AfterFunc(shared.Conf.Stressor.Interval, stressor.do)
	}
}

func (stressor *stressorImp) createIdentity() (string, error) {
	buffer := make([]byte, 32)
	if _, err := rand.Read(buffer); err != nil {
		return "", fmt.Errorf("failed to read random: %v", err)
	}
	return shared.Conf.Stressor.IdentityPrefix + hex.EncodeToString(buffer), nil
}

func (stressor *stressorImp) doOne() error {
	err := shared.EnsureEnrolled(2 * shared.Conf.Stressor.TimeLimit)
	if err != nil {
		return err
	}

	// makes sure that the key material we just checked is loaded
	// by the newContext() calls below.
	err = shared.Pep.SaveSecrets()
	if err != nil {
		return err
	}

	identity, err := stressor.createIdentity()
	if err != nil {
		return err
	}

	// do not use the global context 'shared.Pep' for stressing as
	// that would interfere more heavily with the other checks
	writerCtx := shared.NewPepContext()
	defer writerCtx.Close()

	// the ensureEnrolled above ensures that writerCtx will be enrolled too,
	// since both contexts read their secrets from the same file.

	pp, err := shared.Pep.DerivePolymorphicPseudonym(identity)
	if err != nil {
		return err
	}

	// Write file
	w, err := writerCtx.CreateFile(pp, shared.Conf.Stressor.Column)
	if err != nil {
		return fmt.Errorf("failed to create PEP-file: %v", err)
	}

	hasher := sha256.New()

	_, err = io.Copy(w, io.TeeReader(
		io.LimitReader(rand.Reader,
			int64(shared.Conf.Stressor.FileSize)),
		hasher))
	if err != nil {
		return fmt.Errorf("failed to write PEP-file: %v", err)
	}

	err = w.Close()
	if err != nil {
		return fmt.Errorf("failed to close PEP-file: %v", err)
	}
	
	hash := hasher.Sum(nil)
	o := pep.EnumerateAndOpenFilesOptions{
		PolymorphicPseudonyms: []*pep.Triple{pp},
		Columns:               []string{shared.Conf.Stressor.Column},
	}

	// Read file conf.Stressor.ReadCount times
	//   by conf.Stressor.ReaderCount readers.
	resultc := make(chan error) // results from the readers
	taskc := make(chan uint)    // reads for the readers

	doTask := func(readerCtx *pep.Context, taskNo uint) error {
		if shared.Conf.Stressor.ReEnroll {
			err := readerCtx.EnrollUser(
				shared.TokenFile.OAuthToken)
			if err != nil {
				return fmt.Errorf("failed to re-enroll: %w", err)
			}
		}

		fileReaders, _, err := readerCtx.HistoryAndOpenFiles(o)
		if err != nil {
			return fmt.Errorf(
				"failed to read PEP-file: %v", err)
		}

		if len(fileReaders) == 0 {
			return fmt.Errorf("PEP-file vanished!")
		}

		if len(fileReaders) > 1 {
			return fmt.Errorf("Stored one PEP-file, but " +
				"retrieved several!")
		}

		r := fileReaders[0]

		readerHasher := sha256.New()
		if _, err := io.Copy(readerHasher, r); err != nil {
			return fmt.Errorf(
				"Failed to read PEP-file: %v", err)

		}

		if 0 != bytes.Compare(hash, readerHasher.Sum(nil)) {
			return fmt.Errorf("PEP-file changed!")
		}

		if err := r.Close(); err != nil {
			return fmt.Errorf("Failed to close PEP-file")
		}

		return nil
	}

	doReader := func(readerNo uint) error {
		if shared.Args.TestStressorFail && readerNo == 0 {
			return fmt.Errorf("fake error from " +
				"-test-stressor-fail")
		}

		readerCtx := shared.NewPepContext()
		defer readerCtx.Close()

		for i, more := <-taskc; more; i, more = <-taskc {
			err := doTask(readerCtx, i)
			if err != nil {
				return err
			}
		}

		return nil
	}

	// Start readers
	for rdrNo := uint(0); rdrNo < shared.Conf.Stressor.ReaderCount; rdrNo++ {
		go func(readerNo uint) {
			resultc <- doReader(readerNo)
		}(rdrNo)
	}

	// Send tasks to readers
	go func() {
		for taskNr := uint(0); taskNr < shared.Conf.Stressor.ReadCount; taskNr++ {
			taskc <- taskNr
		}
		close(taskc)
	}()

	// Retrieve results
	activeReaders := shared.Conf.Stressor.ReaderCount
	var multErr error
	for activeReaders > 0 {
		err := <-resultc
		activeReaders--
		if err != nil {
			multErr = multierror.Append(multErr, err)
		}
	}
	close(resultc)

	return multErr
}
