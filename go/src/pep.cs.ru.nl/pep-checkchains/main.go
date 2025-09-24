// pep-checkchains
//
// Commandline tool to retreive the status of the checksum-chains of the
// various pep servers.

package main

import (
	"encoding/hex"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"strings"
	"time"

	"gopkg.in/yaml.v2"

	pep "pep.cs.ru.nl/gopep"
	"pep.cs.ru.nl/gopep/pep_protocol"
)

var (
	constellationFile string
	secretsFile       string

	chainName        string
	maxCheckpointHex string
	serverName       string
	tokenFile        string

	ctx  *pep.Context
	conn *pep_protocol.Conn
)

func main() {
	var err error

	// Parse commandline flags
	flag.StringVar(&constellationFile, "constellation", "constellation.yaml",
		"Constellation file - same as used by the watchdogs")
	flag.StringVar(&secretsFile, "secrets", "secrets.json",
		"Secrets file - same as used by the watchdogs")
	flag.StringVar(&tokenFile, "token", "token.yaml",
		"Token file - same as used by the watchdogs")
	flag.StringVar(&serverName, "server", "",
		"Server to request checksum chains from")
	flag.StringVar(&chainName, "chain", "",
		"Checksum chain to request ")
	flag.StringVar(&maxCheckpointHex, "checkpoint", "",
		"Request XOR of checksums up to the given checkpoint")
	flag.Parse()

	ctx, err = pep.NewContext(constellationFile, secretsFile,
		pep.ContextOptions{
			Timeout: 30 * time.Second,
			Patient: true,
		})
	if err != nil {
		log.Fatalf("Could not create gopep context: %v", err)
	}

	err = ensureEnrolled()
	if err != nil {
		log.Fatalf("Could not enroll: %v", err)
	}

	conns := ctx.Servers()

	var ok bool
	conn, ok = conns[serverName]
	if !ok {
		names := make([]string, 0, len(conns))
		for name := range conns {
			names = append(names, name)
		}

		log.Fatalf("Could not find server '%v' - please choose from %s",
			serverName, strings.Join(names, ", "))
	}

	var maxCheckpoint []byte = nil
	if maxCheckpointHex != "" {
		if chainName == "" {
			log.Fatalf("-checkpoint can be used only in " +
				"combination with -chain, because every chain " +
				"has its own type of checkspoint")
		}
		maxCheckpoint, err = hex.DecodeString(maxCheckpointHex)
		if err != nil {
			log.Fatalf("could not parse -checkpoint: %v", err)
		}
	}

	if chainName != "" {
		query(chainName, maxCheckpoint)
		return
	}

	// Request list of checksum chain names..
	chains, err := pep.ListChecksumChains(ctx.SigningKey(), conn)
	if err != nil {
		log.Fatalf("Failed to list chain names: %v", err)
	}

	// and query them all
	for _, chainName := range chains {
		query(chainName, nil)
	}
}

func ensureEnrolled() error {
	if ctx.EnrolledUntil(time.Now().Add(60 * time.Second)) {
		return nil
	}

	if _, err := os.Stat(tokenFile); os.IsNotExist(err) {
		return fmt.Errorf("could not find token file: %s", tokenFile)
	}

	buf, err := ioutil.ReadFile(tokenFile)
	if err != nil {
		return fmt.Errorf("could not read token file (%s): %w",
			tokenFile, err)
	}

	var token_file struct {
		OAuthToken string `yaml:"OAuthToken"`
	}
	err = yaml.Unmarshal(buf, &token_file)
	if err != nil {
		return fmt.Errorf("could not parse token file (%s): %w",
			tokenFile, err)
	}

	if token_file.OAuthToken == "" {
		return fmt.Errorf("token file (%s): 'OAuthToken' field empty",
			tokenFile)
	}

	err = ctx.EnrollUser(token_file.OAuthToken)
	if err != nil {
		return err
	}

	err = ctx.SaveSecrets()
	if err != nil {
		return fmt.Errorf("could not save secrets: %w", err)
	}

	return nil
}

func query(name string, checkpoint []byte) {
	xorredChecksums, maxCheckpoint, err :=
		pep.QueryChecksumChain(ctx.SigningKey(), conn, name, checkpoint)
	if err != nil {
		log.Fatalf(" request for chain %s failed: %v", name, err)
	}
	fmt.Printf("%s @%x checksum: %x\n", name, maxCheckpoint, xorredChecksums)
}
