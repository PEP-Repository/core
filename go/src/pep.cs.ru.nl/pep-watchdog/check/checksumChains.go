package check

import (
	"bytes"
	"compress/gzip"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"path"
	"time"

	"github.com/bwesterb/bolthold"

	pep "pep.cs.ru.nl/gopep"
	"pep.cs.ru.nl/gopep/pep_protocol"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

// Structure to store old checksums in the database
type ChecksumChainsRecord struct {
	Records map[string]ChecksumChainRecord
	At      time.Time
}

type ChecksumChainRecord struct {
	Checkpoint []byte `yaml:",flow"`
	Checksum   []byte `yaml:",flow"`
}

func ChecksumChains(c Context) {
	if !shared.Pep.Enrolled() {
		return
	}

	key := "records" // we only have one record

	// Fetch old state from database
	var records ChecksumChainsRecord
	if err := shared.Db.Get(key, &records); err == bolthold.ErrNotFound {
		log.Printf("Warning: no ChecksumChainsRecord found in database.  First run?")
		records.Records = make(map[string]ChecksumChainRecord)
	}
	records.At = time.Now()

	// Check each server
	for name, conn := range shared.Pep.Servers() {
		ChecksumChainOf(c, name, conn, &records)
	}

	// Store result in database
	if err := shared.Db.Upsert(key, &records); err != nil {
		c.Issuef("db: failed to store ChecksumChainsRecord: %v", err)
	}

	// Store result in archive
	if err := archive(&records); err != nil {
		c.Issuef("watchdog's checksums archive: %v", err)
	}
}

func ChecksumChainOf(
	c Context,
	name string,
	conn *pep_protocol.Conn,
	records *ChecksumChainsRecord) {

	var sk *pep.CertifiedSigningPrivateKey = shared.Pep.SigningKey()
	if sk == nil {
		c.Issuef("Could not check checksumchains for the %s, "+
			"because enrolment failed", name)
		return
	}
	chains, err := pep.ListChecksumChains(sk, conn)
	if err != nil {
		// TODO for the moment we ignore the KS and TS not replying
		//      to a ListChecksumChains.
		if name == "KeyServer" || name == "Transcryptor" {
			return
		}
		c.Issuef("%s: ListChecksumChains: %v", name, err)
		return
	}
	for _, chain := range chains {
		ChecksumChain(c, name, conn, chain, records)
	}
}

func ChecksumChain(
	c Context,
	name string,
	conn *pep_protocol.Conn,
	chain string,
	records *ChecksumChainsRecord) {

	key := fmt.Sprintf("%s-%s", name, chain)
	record, recordExists := records.Records[key]
	sk := shared.Pep.SigningKey()

	if recordExists {
		// Check old value
		checksum2, _, err := pep.QueryChecksumChain(
			sk, conn, chain, record.Checkpoint)
		if err != nil {
			c.Issuef("%s: checksum chain %s: %v", name, chain, err)
		} else if !bytes.Equal(checksum2, record.Checksum) {
			c.PersistentIssuef(
				"%s: checksum for chain %s@%x changed:"+
					" %x != %x",
				name, chain, record.Checkpoint,
				record.Checksum, checksum2)
		}
	} else {
		log.Printf("%s: new checksum chain %s", name, chain)
	}

	// Get new value
	newChecksum, newCheckpoint, err := pep.QueryChecksumChain(sk, conn, chain, nil)
	if err != nil {
		c.Issuef("%s: checksum chain %s: %v", name, chain, err)
		return
	}

	if !recordExists {
		log.Printf("  checksum %s@%x is %x", chain, newCheckpoint, newChecksum)
	}

	// And store it
	record.Checksum = newChecksum
	record.Checkpoint = newCheckpoint
	records.Records[key] = record
}

func archive(records *ChecksumChainsRecord) error {
	data, err := json.Marshal(records)
	if err != nil {
		return fmt.Errorf("json marshalling failed: %w", err)
	}

	f, err := archiveOpen()
	if err != nil {
		return err
	}
	defer f.Close()

	if _, err := fmt.Fprintln(f, string(data)); err != nil {
		return fmt.Errorf("failed to write record to archive: %w", err)
	}

	return nil
}

func archiveOpen() (*os.File, error) {
	t := time.Now()

	pathToday := archivePath(t)
	f, err := os.OpenFile(pathToday, os.O_APPEND|os.O_WRONLY, 0)
	if err == nil {
		return f, nil
	}
	if !os.IsNotExist(err) {
		return nil, err
	}

	// check for uncompressed files, but no more than one week back
	for i := 0; i < 7; i++ {
		t = t.AddDate(0, 0, -1)
		path := archivePath(t)
		if _, err := os.Stat(path + ".gz"); err == nil {
			break
		}
		if _, err := os.Stat(path); err != nil {
			continue
		}

		if err := compress(path, path+".gz"); err != nil {
			return nil, fmt.Errorf(
				"failed to compress %v: %w", path, err)
		}
		if err := os.Remove(path); err != nil {
			return nil, fmt.Errorf(
				"failed to remove %v: %w", path, err)
		}
	}

	return os.OpenFile(pathToday, os.O_CREATE|os.O_WRONLY, 0644)
}

const archiveTimeFormat = "2006-01-02"

func archivePath(t time.Time) string {
	return path.Join(*shared.Conf.ChecksumsArchive,
		t.Format(archiveTimeFormat))
}

func compress(src, dst string) error {
	inf, err := os.Open(src)
	defer inf.Close()
	if err != nil {
		return err
	}

	outf, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY, 0644)
	defer outf.Close()
	if err != nil {
		return err
	}

	w, err := gzip.NewWriterLevel(outf, gzip.BestCompression)
	defer w.Close()
	if err != nil {
		return err
	}

	if _, err := io.Copy(w, inf); err != nil {
		return err
	}

	return nil
}
