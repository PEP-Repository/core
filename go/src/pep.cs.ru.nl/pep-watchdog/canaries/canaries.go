package canaries

import (
	"fmt"
	"time"

	pep "pep.cs.ru.nl/gopep"

	"github.com/bwesterb/bolthold"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

// Record of a canary file stored in the bolthold
// We use the name "CanaryFileRecord" instead of just "Record" to
// get bolthold to use the correct bucket name.  Alternatively, we could
// have CanaryFileRecord implement the Storer interface, but that's too much
// effort for a shorter name.
type CanaryFileRecord struct {
	BoltID      uint `boltholdKey:"ID"`
	Created     time.Time
	Identifier2 string // "Identifier" is taken by old uint64 ids
	Value       []byte `yaml:",flow"`
}

func Get() (
	records []CanaryFileRecord,
	unknownCanaries []CanaryFileRecord,
	missingCanaries []CanaryFileRecord,
	err error) {

	expectedValues := make(map[string]int)

	// Retreive list of canary files from local database
	err = shared.Db.Find(&records, nil)
	if err != nil {
		return nil, nil, nil, fmt.Errorf("db.Find: %v", err)
	}

	//  Retreive canary files from storage facility
	pp, err := shared.Pep.DerivePolymorphicPseudonym(
		shared.Conf.CanaryFile.Identity)
	if err != nil {
		return nil, nil, nil, fmt.Errorf("DerivePolymorphicPseudonym: %v", err)
	}

	bufs, fileInfos, err := shared.Pep.HistoryAndReadFiles(
		pep.EnumerateAndOpenFilesOptions{
			PolymorphicPseudonyms: []*pep.Triple{pp},
			Columns: []string{
				shared.Conf.CanaryFile.Tag},
		})
	if err != nil {
		return nil, nil, nil, fmt.Errorf("HistoryAndReadFiles: %v", err)
	}

	// Check if files are missing
	for index, buf := range bufs {
		expectedValues[string(buf)] = index
	}
	for _, r := range records {
		if _, ok := expectedValues[string(r.Value)]; !ok {
			missingCanaries = append(missingCanaries, r)
		} else {
			expectedValues[string(r.Value)] = -1
		}
	}

	for val, index := range expectedValues {
		if index >= 0 {
			fileInfo := fileInfos[index]
			unknownCanaries = append(unknownCanaries,
				CanaryFileRecord{
					Created:     fileInfo.Metadata.Timestamp,
					Identifier2: fileInfo.Identifier,
					Value:       []byte(val),
				})
		}
	}

	return
}

func Sync() error {
	_, unknownCanaries, missingCanaries, err := Get()
	if err != nil {
		return err
	}

	for _, r := range unknownCanaries {
		err = shared.Db.Insert(bolthold.NextSequence(), &r)
		if err != nil {
			return err
		}
	}
	for _, r := range missingCanaries {
		err = shared.Db.Delete(r.BoltID, r)
		if err != nil {
			return err
		}
	}
	return nil
}
