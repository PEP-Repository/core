package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/dustin/go-humanize"
	"gopkg.in/gomail.v2"
	"gopkg.in/yaml.v2"
)

type Conf struct {
	BindAddr      string        `yaml:"bindAddr"`
	CheckInterval time.Duration `yaml:"interval"`     // interval  between checks
	MailInterval  time.Duration `yaml:"grace"`        // interval between emails
	WatchdogUrls  []string      `yaml:"watchdogUrls"` // watchdogs to check
	Mailer        MailerConf    `yaml:"mailer"`
}

type MailerConf struct {
	Host     string `yaml:"host"`
	Port     int    `yaml:"port"`
	Username string `yaml:"username"`
	Password string `yaml:"password"`
	From     string `yaml:"from"`
	To       string `yaml:"to"`
}

var (
	conf      Conf
	ticker    *time.Ticker
	lastCheck time.Time
	lastMail  time.Time
	mailer    *gomail.Dialer
)

func main() {
	var confPath string
	conf = Conf{
		BindAddr:      "localhost:8181",
		WatchdogUrls:  []string{"http://localhost:8080"},
		CheckInterval: 5 * time.Minute,
		MailInterval:  24 * time.Hour,
		Mailer: MailerConf{
			Host: "localhost",
			Port: 25,
			From: "PEP-Watchdog Watchdog <watchdog-watchdog@pep.cs.ru.nl>",
			To:   "pep@science.ru.nl",
		},
	}

	flag.StringVar(&confPath, "config", "config.yaml",
		"Path to configuration file")
	flag.Parse()

	if _, err := os.Stat(confPath); os.IsNotExist(err) {
		fmt.Printf("Error: could not find configuration file: %s\n\n", confPath)
		fmt.Printf("Example configuration file:\n\n")
		buf, _ := yaml.Marshal(&conf)
		fmt.Printf("%s\n", buf)
		return
	}

	buf, err := ioutil.ReadFile(confPath)
	if err != nil {
		log.Fatalf("Could not read %s: %v", confPath, err)
	}
	err = yaml.Unmarshal(buf, &conf)
	if err != nil {
		log.Fatalf("Could not parse config file: %v", err)
	}

	mailer = gomail.NewPlainDialer(conf.Mailer.Host, conf.Mailer.Port,
		conf.Mailer.Username, conf.Mailer.Password)

	http.HandleFunc("/", handler)

	log.Printf("Will check status every %s", conf.CheckInterval)
	ticker = time.NewTicker(conf.CheckInterval)

	go func() {
		for {
			check()
			<-ticker.C
		}
	}()

	log.Printf("Listening on %s ...", conf.BindAddr)
	log.Fatal(http.ListenAndServe(conf.BindAddr, nil))
}

func checkWatchdog(url string) error {
	resp, err := http.Get(url + "/status")
	if err != nil {
		return fmt.Errorf("Couldn't reach watchdog: %v", err)
	}

	buf, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("Couldn't reach watchdog: %v", err)
	}

	var status struct {
		Deadline time.Time
		Now      time.Time
	}
	err = json.Unmarshal(buf, &status)
	if err != nil {
		return fmt.Errorf("Couldn't parse watchdog response: %v", err)
	}

	if status.Now.After(status.Deadline) {
		return errors.New("Watchdog is running, but didn't finish a check in time")
	}

	return nil
}

func check() {
	var errs []string

	for _, url := range conf.WatchdogUrls {
		log.Printf("Checking up on %s ...", url)
		err := checkWatchdog(url)

		if err != nil {
			errs = append(errs, fmt.Sprintf("%s: %v", url, err))
		}
	}
	if len(errs) == 0 {
		log.Printf("    ... ok: everything is fine.")
		lastCheck = time.Now()
		return
	}

	log.Printf("  there were errors:")
	for _, err := range errs {
		log.Printf("    %s", err)
	}

	if lastMail.Add(conf.MailInterval).After(time.Now()) {
		return
	}

	m := gomail.NewMessage()
	m.SetHeader("From", conf.Mailer.From)
	m.SetHeader("To", conf.Mailer.To)
	m.SetHeader("Subject", "Watchdog(s) not running")

	msg := fmt.Sprintf(
		"There is a problem with the watchdogs:\n\n"+
			"  %v\n\n"+
			"I will report again %s\n\n"+
			"    PEP-Watchdog Watchdog",
		strings.Join(errs, "\n  "),
		humanize.Time(time.Now().Add(conf.MailInterval)))

	m.SetBody("text/plain", msg)
	if err := mailer.DialAndSend(m); err != nil {
		log.Printf("Failed to send e-mail: %s", err)
	}

	lastMail = time.Now()
}

// Handles HTTP GET on /status, used by the pep-watchdog-watchdog.
func handler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(
		&struct {
			Deadline time.Time
			Now      time.Time
		}{
			Deadline: time.Now().Add(conf.CheckInterval * 2),
			Now:      time.Now(),
		})
}
