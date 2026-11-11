package canaries

import (
	"fmt"
	"io/ioutil"
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

func readIfUnpurged(info *pep.FileInfo) ([]byte, error) {
	if info.PurgedAt != nil {
		return nil, nil
	}
	
	fr, err := shared.Pep.OpenFile(info)
	if err != nil {
		return nil, fmt.Errorf("OpenFile: %v", err)
	}
	
	buf, err := ioutil.ReadAll(fr)
	if err != nil {
		return nil, fmt.Errorf("ioutil.ReadAll: %v", err)
	}
	
	return buf, nil
}

func Get() (
	records []CanaryFileRecord,
	unknownCanaries []CanaryFileRecord,
	missingCanaries []CanaryFileRecord,
	err error) {

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
	
	// Check if files are missing or corrupted
	for index, r := range records {
		if index >= len(fileInfos) {
			missingCanaries = append(missingCanaries, r)
		} else {
			buf, err := readIfUnpurged(&fileInfos[index])
			if err != nil {
				return nil, nil, nil, err
			}
			
			if buf != nil { // Can only check contents if the entry wasn't purged
				if string(buf) != string(r.Value) {
					missingCanaries = append(missingCanaries, r) // TODO: report corrupted instead
				}
			}
		}
	}

	for index := len(records); index < len(fileInfos); index++ {
		fileInfo := fileInfos[index]
		val, err := readIfUnpurged(&fileInfo)
		if err != nil {
			return nil, nil, nil, err
		}

		unknownCanaries = append(unknownCanaries,
			CanaryFileRecord{
				Created:     fileInfo.Metadata.Timestamp,
				Identifier2: fileInfo.Identifier,
				Value:       val,
			})
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
