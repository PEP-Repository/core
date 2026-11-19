package canaries

import (
	"fmt"
	"io/ioutil"
	"time"

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

	// Retrieve list of canary files from local database
	err = shared.Db.Find(&records, nil)
	if err != nil {
		return nil, nil, nil, fmt.Errorf("db.Find: %v", err)
	}

	//  Retrieve canary files from storage facility
	pp, err := shared.Pep.DerivePolymorphicPseudonym(
		shared.Conf.CanaryFile.Identity)
	if err != nil {
		return nil, nil, nil, fmt.Errorf("DerivePolymorphicPseudonym: %v", err)
	}

	fileInfos, err := shared.Pep.FileHistory(
		pp, shared.Conf.CanaryFile.Tag)
	if err != nil {
		return nil, nil, nil, fmt.Errorf("FileHistory: %v", err)
	}

  for index, info := range fileInfos {
		if info.RedactedAt == nil {
			// We don't check for tombstones because we assume that no one pepcli deletes the shared.Conf.CanaryFile.Tag column
			fr, err := shared.Pep.OpenFile(&info)
			if err != nil {
				return nil, nil, nil, fmt.Errorf("OpenFile: %v", err)
			}
			
			buf, err := ioutil.ReadAll(fr)
			if err != nil {
				return nil, nil, nil, fmt.Errorf("ioutil.ReadAll: %v", err)
			}
			expectedValues[string(buf)] = index
		}
	}

	for _, r := range records {
		if _, ok := expectedValues[string(r.Value)]; !ok {
			// Server did not produce a canary (version) for this record, or the version was redacted
			missingCanaries = append(missingCanaries, r)
		} else {
			// Received this record's canary version payload from the server
			expectedValues[string(r.Value)] = -1
		}
	}

	for val, index := range expectedValues {
		if index >= 0 { // We received a(n unredacted) cell version that we don't have a record for
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
