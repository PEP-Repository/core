package main

import (
	"fmt"
	"log"
	"reflect"

	"pep.cs.ru.nl/pep-watchdog/canaries"
	"pep.cs.ru.nl/pep-watchdog/check"
	"pep.cs.ru.nl/pep-watchdog/shared"
	"pep.cs.ru.nl/pep-watchdog/state"

	"gopkg.in/yaml.v2"
)

type Exported struct {
	PersistentIssues []state.PersistentIssueRecord
	Canaries         []canaries.CanaryFileRecord
	ChecksumChains   []check.ChecksumChainsRecord
}

func exportDB() {
	var e Exported

	load := func(target interface{}) {
		if err := shared.Db.Find(target, nil); err != nil {
			log.Fatalf("failed to load %v from database: %v",
				reflect.TypeOf(target), err)
		}
	}

	load(&e.PersistentIssues)
	load(&e.Canaries)
	load(&e.ChecksumChains)

	buf, err := yaml.Marshal(&e)
	if err != nil {
		log.Fatalf("failed to marshal data to yaml: %v", err)
	}

	fmt.Print(string(buf))
}
