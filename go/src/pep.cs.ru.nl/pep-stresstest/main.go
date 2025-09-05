// Probes performance of PEP services under load
package main

import (
	"encoding/hex"
	"flag"
	"fmt"
	"log"
	"os"
	"sync"
	"time"

	"github.com/bmizerany/perks/quantile"
	"github.com/olekukonko/tablewriter"

	"pep.cs.ru.nl/gopep"
)

type testFunc func()

var (
	// Flags
	constellationFile string
	secretsFile       string
	token             string
	tokenSecret       string

	// Context
	ctx *pep.Context

	// Precomputed test state
	ticketRequestPP *pep.Triple

	// The tests
	tests = map[string]testFunc{
		// "20-small-reads": smallReads,
		"ticket-request": ticketRequest,
	}
)

func main() {
	setupContext()
	prepareTests()

	for name, f := range tests {
		fmt.Printf("\n\n%s\n===========================================\n", name)
		runTest(f)
	}
}

func ticketRequest() {
	_, err := ctx.AM().RequestTicket2(ctx.Secrets().SigningKey,
		pep.TicketRequest{
			Modes:                 []string{"read"},
			PolymorphicPseudonyms: []*pep.Triple{ticketRequestPP},
			Columns:               []string{"ParticipantInfo"},
		})
	if err != nil {
		log.Fatalf("ticketRequest() failed: %v", err)
	}
}

func smallReads() {
	pp, err := ctx.DerivePolymorphicPseudonym("20-short-reads-stresstest")
	if err != nil {
		log.Fatalf("smallReads() failed: %v", err)
		return
	}
	
	_, _, err := ctx.EnumerateAndReadFiles(pep.EnumerateAndOpenFilesOptions{
		PolymorphicPseudonyms: []*pep.Triple{pp},
		Columns:               []string{"ParticipantInfo"},
	})
	if err != nil {
		log.Fatalf("smallReads() failed: %v", err)
	}
}

func runTest(test testFunc) {
	table := tablewriter.NewWriter(os.Stdout)
	table.SetHeader([]string{
		"#workers",
		".5q",
		".9q",
		".99q",
		"avg",
		"wall avg",
	})
	for nWorkers := 1; nWorkers <= 6; nWorkers++ {
		var lock sync.Mutex
		var wg sync.WaitGroup
		var totalDur float64
		q := quantile.NewTargeted(0.50, 0.90, 0.99)
		wg.Add(nWorkers)

		fullStart := time.Now()

		for w := 0; w < nWorkers; w++ {
			go func() {
				for i := 0; i < 120/nWorkers; i++ {
					start := time.Now()
					test()
					dur := time.Since(start).Seconds()
					lock.Lock()
					totalDur += dur
					q.Insert(dur)
					lock.Unlock()
				}
				wg.Done()
			}()
		}

		wg.Wait()
		wallTotalDur := time.Since(fullStart).Seconds()

		table.Append([]string{
			fmt.Sprintf("%d", nWorkers),
			fmt.Sprintf("%.4f", q.Query(0.5)),
			fmt.Sprintf("%.4f", q.Query(0.9)),
			fmt.Sprintf("%.4f", q.Query(0.99)),
			fmt.Sprintf("%.4f", totalDur/120),
			fmt.Sprintf("%.4f", wallTotalDur/120),
		})
	}

	table.Render()
}

func prepareTests() {
	// shortReads
	pp, err := ctx.DerivePolymorphicPseudonym("20-short-reads-stresstest")
	if err != nil {
		log.Fatalf("%v", err)
	}
	fs, err := ctx.EnumerateFiles(pep.EnumerateFilesOptions{
		ColumnGroups:          []string{"ShortPseudonyms"},
		PolymorphicPseudonyms: []*pep.Triple{pp},
	})
	if err != nil {
		log.Fatalf("%v", err)
	}
	if len(fs) == 0 {
		for i := 0; i < 20; i++ {
			_, err = ctx.CreateFileFromBytes(pp, "ShortPseudonym.Visit1.PBMC",
				[]byte("short read 1"))
			if err != nil {
				log.Fatalf("%v", err)
			}
		}
	} else if len(fs) != 20 {
		log.Fatalf("Wrong number of test files under read-stresstest PP")
	}

	// ticketRequest
	ticketRequestPP, err = ctx.DerivePolymorphicPseudonym("ticket-request-pp")
	if err != nil {
		log.Fatalf("%v", err)
	}
}

func setupContext() {
	var err error

	flag.StringVar(&constellationFile, "constellation",
		"local.constellation.yaml",
		"Constellation file to use")
	flag.StringVar(&secretsFile, "secrets", "secrets.json",
		"File to store secrets")
	flag.StringVar(&token, "token", "",
		"Token to use to enroll")
	flag.StringVar(&tokenSecret, "token-secret", "",
		"OAuthTokenSecret used to generate a token")
	flag.Parse()

	ctx, err = pep.NewContext(constellationFile, secretsFile,
		pep.ContextOptions{})
	if err != nil {
		log.Fatalf("NewContext: %v", err)
	}

	if !ctx.Enrolled() {
		if tokenSecret == "" && token == "" {
			log.Fatalf("Either a -token or -token-secret must be specified")
		}

		if token == "" {
			parsedToken, err := hex.DecodeString(tokenSecret)
			if err != nil {
				log.Fatalf("token-secret: %v", err)
			}
			token = pep.CreateOAuthToken(parsedToken, "pep-stresstest",
				"Research Assessor", time.Hour*24*356)
		}

		err = ctx.EnrollUser(token)
		if err != nil {
			log.Fatalf("EnrollUser: %v", err)
		}
		err = ctx.SaveSecrets()
		if err != nil {
			log.Fatalf("SaveSecrets: %v", err)
		}
	}
}
