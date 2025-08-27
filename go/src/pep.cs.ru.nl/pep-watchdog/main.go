package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"sync"
	"time"

	// Much of the watchdog's core business is stored in
	// the following two subpackages.
	"pep.cs.ru.nl/pep-watchdog/shared" // conf, args, db and pep conns..
	"pep.cs.ru.nl/pep-watchdog/state"  // issues, last-check, etc..

	"pep.cs.ru.nl/pep-watchdog/canaries"
	"pep.cs.ru.nl/pep-watchdog/check"
	"pep.cs.ru.nl/pep-watchdog/http"
	"pep.cs.ru.nl/pep-watchdog/mail"

	"github.com/bwesterb/bolthold"
	"github.com/dustin/go-humanize"
	"github.com/prometheus/client_golang/prometheus"
)

var (
	ticker *time.Ticker

	metrIssueCount = prometheus.NewGauge(prometheus.GaugeOpts{
		Name: "pep_watchdog_issue_count",
		Help: "Number of open issues",
	})

	metrCheckDuration = prometheus.NewSummaryVec(prometheus.SummaryOpts{
		Name:       "pep_watchdog_check_duration_seconds",
		Help:       "Time it took to check issues",
		Objectives: map[float64]float64{0.5: 0.05, 0.9: 0.01, 0.99: 0.001},
	}, []string{"check"})

	started time.Time = time.Now()

	metrUptime = prometheus.NewGaugeFunc(prometheus.GaugeOpts{
		Name: "pep_uptime_seconds",
		Help: "Uptime in seconds",
	}, func() float64 { return time.Now().Sub(started).Seconds() })

	checks = map[string]check.Func{
		"token-expiry":             check.TokenExpiry,
		"pep-enrollment":           check.Enrollment,
		"https-certificate-expiry": check.HTTPSCertificateExpiry,
		"tls-certificate-expiry":   check.TLSCertificateExpiry,
		"server-ping":              check.PEPServers,
		"checksum-chains":          check.ChecksumChains,
		"canary-files":             check.CanaryFiles,
		"watchdog-watchdog":        check.WatchdogWatchdog,
		"pep-certificate-expiry":   check.PEPCertificateExpiry,
		"pep-versions":             check.PEPVersions,
		"pipelines":                check.Pipelines,
		// Conditionally added:
		//
		// "test-hang"
		// "stressor"
		//
		// See below.
	}
)

func init() {
	prometheus.MustRegister(
		metrIssueCount,
		metrCheckDuration,
		metrUptime,
	)
}

func main() {
	shared.Setup() // parse args, load conf, etc.

	args := &shared.Args
	conf := &shared.Conf

	if args.TestHang {
		checks["test-hang"] = func(c check.Context) { select {} }
	}

	if args.TestStressorFail || args.InstantStressor {
		if !conf.Stressor.Enabled {
			log.Fatalf("error: no stressor "+
				"configured in %v", args.ConfPath)
		}
	}

	if conf.Url == nil {
		url := "http://" + conf.BindAddr
		conf.Url = &url
	}

	if conf.SelfCheckAddr == "" {
		log.Fatalf("Error: %v: 'selfCheckAddr' not set", args.ConfPath)
	}

	if conf.SelfCheckBindAddr == "" {
		log.Fatalf("Error: %v: 'selfCheckBindAddr' not set", args.ConfPath)
	}

	var err error
	shared.Db, err = bolthold.Open(conf.DbFile, 0600, nil)
	if err != nil {
		log.Fatalf("Could not open database %s: %v", conf.DbFile, err)
	}
	defer shared.Db.Close()

	if args.Command == shared.ExportDB {
		exportDB()
		os.Exit(0)
	}

	if args.Command == shared.ClearPersistentIssues {
		state.ClearPersistentIssues()
		log.Printf("Cleared persistent issues")
		os.Exit(0)
	}

	mail.Setup()
	if args.Command == shared.TestMail {
		mail.Test()
		os.Exit(0)
	}

	shared.Pep = shared.NewPepContext()
	defer shared.Pep.Close() // TODO graceful shutdown

	if args.Command != shared.OneShot {
		checkHostname()
	}

	if args.Command == shared.SyncCanaries {
		err := shared.EnsureEnrolled(2 * conf.Interval)
		if err != nil {
			log.Fatalf("Syncing canaries: %v", err)
		}

		err = canaries.Sync()
		if err != nil {
			log.Fatalf("Error syncing canary files: %v", err)
		}

		os.Exit(0)
	}

	// setup stressor
	if conf.Stressor.Enabled &&
		(args.Command != shared.OneShot || args.InstantStressor) {
		// on -oneshot, 'NewStressor()' will run the stressor
		// immediately
		shared.Stressor = NewStressor()
		checks["stressor"] = check.Stressor
	}

	if args.Command == shared.OneShot {
		doCheck()

		persistentIssues := state.GetPersistentIssues()

		ret := 0

		func() {
			// Locking is probably not necessary here,
			// because the HTTP server is not running,
			// but we do not want to set a bad example for
			// future code modifications.
			state.Mux.RLock()
			defer state.Mux.RUnlock()

			if len(state.Issues) != 0 {
				fmt.Printf("\n")
				fmt.Printf("The following issues " +
					"were found:\n\n")
				for msg, _ := range state.Issues {
					fmt.Printf(" * %s\n", msg)
				}
				ret = 1
			}
		}()

		if len(persistentIssues) != 0 {
			fmt.Printf("\n")
			fmt.Printf("Persistent issues:\n\n")
			for _, pi := range persistentIssues {
				fmt.Printf(" * %s:  %s\n",
					humanize.Time(pi.At),
					pi.Message)
			}
			ret = 1
		}

		os.Exit(ret)
	}

	if args.Command != shared.None {
		log.Fatalf("Error: -%v not implemented", shared.Args.Command)
	}

	log.Printf("Will check status every %s", conf.Interval)
	ticker = time.NewTicker(conf.Interval)

	go func() {
		for {
			doCheck()
			<-ticker.C
		}
	}()

	http.ListenAndServe()

}

func withContext(ctx check.Context, f check.Func) error {
	ch := make(chan bool, 1)

	go func() {
		f(ctx)

		ch <- true
	}()

	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-ch:
		return nil
	}
}

func doCheck() {
	var initialCheck bool

	func() {
		state.Mux.Lock()
		defer state.Mux.Unlock()

		state.Stats.CheckInProgress = true
		state.Stats.LastCheck = time.Now()
		state.Stats.CheckStarted = time.Now()

		initialCheck = state.Stats.InitialCheck
	}()

	log.Printf("Running checks ...")

	timeoutCtx, cancel := context.WithTimeout(context.Background(), shared.Conf.Timeout)
	defer cancel()
	c := &CheckContext{
		Context:   timeoutCtx,
		IssueChan: make(chan NewIssue, 5),
	}

	shared.EnsureEnrolled(2 * shared.Conf.Interval)
	// Not all checks require us to be succesfully enrolled,
	// so on enrolment failure we do not abort.

	if shared.Args.TestIssue {
		c.Issuef("Fake issue from -test-issue")
	}
	if shared.Args.TestPersistentIssue && initialCheck {
		c.PersistentIssuef("Fake persistent issue " +
			"from -test-persistent-issue")
	}

	// Launch checks in parallel
	c.Wg.Add(len(checks))
	for name, checkFunc := range checks {
		_, skipped := shared.Args.Skip[name]
		if !skipped {
			go func(name string, checkFunc check.Func) {
				start := time.Now()
				err := withContext(c, checkFunc)
				if err != nil {
					if err == context.DeadlineExceeded {
						c.PersistentIssuef("%s timed out. (timeout is %s)", name, shared.Conf.Timeout)
					} else {
						c.PersistentIssuef("Error during check %s: %s", name, err.Error())
					}
				} else {
					metrCheckDuration.With(prometheus.Labels{
						"check": string(name),
					}).Observe(time.Since(start).Seconds())
				}
				c.Wg.Done()
			}(name, checkFunc)
		} else {
			c.Wg.Done()
		}
	}

	var curIssues []string // current non-persistent issues

	// Collect issues
	go func() {
		for {
			issue, ok := <-c.IssueChan
			if !ok {
				break
			}
			if !issue.persistent {
				curIssues = append(curIssues, issue.message)
			} else {
				state.RecordPersistentIssue(issue.message)
			}
		}
		c.Wg.Done()
	}()

	c.Wg.Wait() // wait for workers
	log.Printf("All workers are finished")
	c.Wg.Add(1)
	close(c.IssueChan)
	c.Wg.Wait() // wait for issue collector

	// update state
	func() {
		state.Mux.Lock()
		defer state.Mux.Unlock()

		// update issue records
		for _, meta := range state.Issues {
			meta.Active = false
		}

		now := time.Now()

		for _, msg := range curIssues {
			if _, ok := state.Issues[msg]; !ok {
				state.Issues[msg] = state.NewIssueMeta(now)
			} else {
				state.Issues[msg].LastSighting = now
				state.Issues[msg].Active = true
			}
		}

		state.SetIssueStats()

		if state.Stats.MustReport() &&
			shared.Args.Command == shared.None {

			report := state.CompileReport(true) // create under lock
			go mail.Issues(report)
		}

		state.Stats.InitialCheck = false
		metrIssueCount.Set(float64(state.Stats.Active))
		state.Stats.LastCheck = time.Now()
		state.Stats.LastChange = time.Now()
		log.Printf("    ... checks done: %d active "+
			"and %d persistent issue(s)",
			state.Stats.Active,
			state.Stats.Persistent)
		state.Stats.CheckInProgress = false
	}()
}

type NewIssue struct {
	message    string
	persistent bool
}

type CheckContext struct {
	context.Context

	IssueChan chan NewIssue
	Wg        sync.WaitGroup
}

func (c *CheckContext) PersistentIssuef(fmts string, args ...interface{}) {
	select {
	case <-c.Done():
		log.Printf("Encountered persistent issue, but check was already canceled: %s\nPersistent issue: %s",
			c.Err().Error(), fmt.Sprintf(fmts, args...))
	case c.IssueChan <- NewIssue{
		message:    fmt.Sprintf(fmts, args...),
		persistent: true,
	}:
	}
}

func (c *CheckContext) Issuef(fmts string, args ...interface{}) {
	select {
	case <-c.Done():
		log.Printf("Encountered issue, but check was already canceled: %s\nIssue: %s",
			c.Err().Error(), fmt.Sprintf(fmts, args...))
	case c.IssueChan <- NewIssue{
		message:    fmt.Sprintf(fmts, args...),
		persistent: false,
	}:
	}
}
