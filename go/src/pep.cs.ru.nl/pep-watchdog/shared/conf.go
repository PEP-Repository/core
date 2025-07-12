package shared

import (
	"io/ioutil"
	"log"
	"os"
	"path"
	"time"

	"gopkg.in/yaml.v2"
)

var Conf = ConfType{
	ConstellationFile:         "constellation.yaml",
	SecretsFile:               "secrets.json",
	DbFile:                    "watchdog.db",
	BindAddr:                  "localhost:8080",
	Interval:                  5 * time.Minute,
	Timeout:                   5 * time.Minute,
	EphemeralBelow:            20 * time.Minute,
	MaxUnreportedSolvedIssues: 50,
	ProbationPeriod:           1 * time.Hour,
	CanaryFile: CanaryFileConf{
		Interval: time.Hour * 24,
		Identity: "pep-canary-file",
		Tag:      "Canary",
	},
	Stressor: StressorConf{
		StartAfter:     8 * time.Hour,
		Interval:       7 * 24 * time.Hour,
		TimeLimit:      3 * time.Hour,
		FileSize:       1024 * 1024 * 1024,
		ReadCount:      5,
		ReaderCount:    5,
		ReEnroll:       false,
		IdentityPrefix: "stressor-",
		Column:         "Canary", // hacky reuse
	},
	Mailer: OptionalMailerConf{MailerConf: MailerConf{
		Host: "localhost",
		Port: 25,
		From: "PEP Watchdog <watchdog@pep.cs.ru.nl>",
		To:   "pep@science.ru.nl",
	}},
}

type ConfType struct {
	Name string `yaml:"name"`

	ConstellationFile string `yaml:"constellationFile"`
	SecretsFile       string `yaml:"secretsFile"`
	DbFile            string `yaml:"dbFile"`

	// Directory in which previously seen checksums are stored.
	// If not set, then the "checksums" folder in the parent directory of
	// the DbFile (watchdog.db) is used.
	ChecksumsArchive *string `yaml:"checksumsArchive"`

	TokenFile string `yaml:"tokenFile"`
	Token     string `yaml:"token"` // deprecated, but kept for warnings

	BindAddr string        `yaml:"bindAddr"`
	Url      *string       `yaml:"url"`
	Interval time.Duration `yaml:"interval"` // interval  between most checks
	Timeout  time.Duration `yaml:"timeout"`  // timeout for the checks

	// don't immediately report on issues that only last a few minutes
	EphemeralBelow time.Duration `yaml:"ephemeralBelow"`

	// unless there're too many unreported solved issues
	MaxUnreportedSolvedIssues uint `yaml:"maxUnreportedSolvedIssues"`

	// after an issue has been solved, keep it on record for at least this
	// amount of time to see if it might reappear.
	ProbationPeriod time.Duration `yaml:"probationPeriod"`

	WatchdogWatchdogUrl string `yaml:"watchdogWatchdogUrl"`

	SelfCheckAddr     string `yaml:"selfCheckAddr"`
	SelfCheckBindAddr string `yaml:"selfCheckBindAddr"`

	// Configuration of checks
	CheckHTTPSCertificateExpiry []string `yaml:"checkHTTPSCertificateExpiry"`

	CheckTLSCertificateExpiry []string `yaml:"checkTLSCertificateExpiry"`

	CanaryFile CanaryFileConf `yaml:"canaryFile"`
	Stressor   StressorConf   `yaml:"stressor"`

	Mailer OptionalMailerConf `yaml:"mailer"`

	CheckPipelines []PipelineConf `yaml:"checkPipelines"`
}

type CanaryFileConf struct {
	Interval time.Duration `yaml:"interval"`
	Identity string        `yaml:"identity"`
	Tag      string        `yaml:"tag"` // i.e. column name
}

type StressorConf struct {
	Enabled bool `yaml:"enabled"`
	// We use this "Enabled" field so that the stressor is disabled
	// by default.  If we had instead given the conf.Stressor the type
	// *StressorConf, then
	//
	// stressor: null
	//
	// has to be added to all non-acceptance config files.
	//
	// If Enabled, performs the first test after StartAfter,
	// and then waits Interval after each test ends before starting a
	// new one.  Raises an issue if the test takes longer than TimeLimit.
	StartAfter time.Duration `yaml:"startAfter"`
	Interval   time.Duration `yaml:"interval"`
	TimeLimit  time.Duration `yaml:"timeLimit"`

	// The test involves first storing a file of FileSize bytes,
	// and then immediately downloading it ReadCount times,
	// with ReaderCount readers in parallel.
	FileSize    uint `yaml:"fileSize"`
	ReadCount   uint `yaml:"readCount"`
	ReaderCount uint `yaml:"readerCount"`
	ReEnroll    bool `yaml:"reEnroll"` // re-enroll before each read

	// The file is stored under a random identity starting
	// with IdentityPrefix under the column Column.
	IdentityPrefix string `yaml:"identityPrefix"`
	Column         string `yaml:"column"`
}

type MailerConf struct {
	Host     string `yaml:"host"`
	Port     int    `yaml:"port"`
	Username string `yaml:"username"`
	Password string `yaml:"password"`
	From     string `yaml:"from"`
	To       string `yaml:"to"`
}

// We use OptionalMailerConf instead of just MailerConf in order to detect
// whether "mailer:" was actually set in the configuration file (without
// having to give up on its sensible default values).
type OptionalMailerConf struct {
	MailerConf `yaml:",inline,omitempty"`
	isSet      bool
}

func (om *OptionalMailerConf) HasValue() bool {
	return om.isSet
}

func (opt *OptionalMailerConf) UnmarshalYAML(
	unmarshal func(interface{}) error) error {

	opt.isSet = true
	return unmarshal(&opt.MailerConf)
}

type PipelineConf struct {
	Project  string   `yaml:"project"`
	Branches []string `yaml:"branches"`
}

func LoadConf() {
	if _, err := os.Stat(Args.ConfPath); os.IsNotExist(err) {
		buf, _ := yaml.Marshal(&Conf)
		log.Fatalf("Error: could not find configuration file: %s\n\n"+
			"Example configuration file:\n\n%s\n",
			Args.ConfPath, buf)
	}

	buf, err := ioutil.ReadFile(Args.ConfPath)
	if err != nil {
		log.Fatalf("Could not read %s: %v", Args.ConfPath, err)
	}
	err = yaml.Unmarshal(buf, &Conf)
	if err != nil {
		log.Fatalf("Could not parse config file: %v", err)
	}

	checkConfig()
}

func checkConfig() {
	if Conf.ChecksumsArchive == nil {
		rootDir, _ := path.Split(Conf.DbFile)
		dir := path.Join(rootDir, "checksums")
		Conf.ChecksumsArchive = &dir
	}
	if err := os.MkdirAll(*Conf.ChecksumsArchive, 0755); err != nil {
		log.Fatalf("Could not create checksums archive %v: %v",
			*Conf.ChecksumsArchive, err)
	}
}
