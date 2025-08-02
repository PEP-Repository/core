package shared

import (
	"flag"
	"log"
	"strings"
)

var Args ArgsType

// arguments passed to watchdog via command line
type ArgsType struct {
	ConfPath            string
	TestIssue           bool
	TestPersistentIssue bool
	TestHang            bool
	TestStressorFail    bool
	InstantStressor     bool
	Skip                map[string]struct{}
	Command             CommandType
}

type CommandType string

const (
	None                  CommandType = ""
	OneShot                           = "oneshot"
	ClearPersistentIssues             = "clear-persistent-issues"
	SyncCanaries                      = "sync-canaries"
	TestMail                          = "test-mail"
	ExportDB                          = "export-db"
)

func ParseArgs() {
	flag.StringVar(&Args.ConfPath, "config", "config.yaml",
		"Path to configuration file")
	flag.BoolVar(&Args.TestIssue, "test-issue", false,
		"Create a fake (non-persistent) issue to test notifications")
	flag.BoolVar(&Args.TestPersistentIssue, "test-persistent-issue", false,
		"Create a fake persistent issue")
	flag.BoolVar(&Args.TestHang, "test-hang", false,
		"Simulate a hanging check to test the badge")
	flag.BoolVar(&Args.TestStressorFail, "test-stressor-fail", false,
		"Have the stressor fake a failure")
	flag.BoolVar(&Args.InstantStressor, "instant-stressor", false,
		"Run stressor immediately - useful for debugging")

	skipString := ""
	flag.StringVar(&skipString, "skip", "", "Comma separated list of checks to skip")

	commandVars := make(map[CommandType]*bool)

	addCommand := func(cmd CommandType, help string) {
		commandVars[cmd] = new(bool)
		flag.BoolVar(commandVars[cmd], (string)(cmd), false, help)
	}

	addCommand(OneShot, "Run checks once and then exit.\n"+
		"Runs the stressor immediately before these "+
		"checks when -instant-stressor was set.")
	addCommand(ClearPersistentIssues,
		"Mark persistent issues resolved, and quit")
	addCommand(SyncCanaries, "Resolve missing/unknown canaries"+
		" by syncing the watchdog DB with PEP; then quit")
	addCommand(TestMail, "Send test email, and quit.")
	addCommand(ExportDB, "Export database's content as yaml.")

	flag.Parse()

	// set Args.Skip. This is a map, because in go it's easier to see if a key is in a map, than to see if an item is in a slice
	skipSlice := strings.Split(skipString, ",")
	Args.Skip = make(map[string]struct{}, len(skipSlice))
	for _, s := range skipSlice {
		Args.Skip[s] = struct{}{}
	}

	// set Args.Command
	for cmd, isSet := range commandVars {
		if !(*isSet) {
			continue
		}
		if Args.Command != None {
			log.Fatalf("Cannot combine flags -%v and -%v",
				Args.Command, cmd)
		}
		Args.Command = cmd
	}
}
