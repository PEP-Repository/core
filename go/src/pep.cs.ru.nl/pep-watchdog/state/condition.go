package state

import (
	"pep.cs.ru.nl/pep-watchdog/consts"
)

type WatchCon uint

const (
	AllOK                    WatchCon = iota
	InitialCheck                      // White
	HadRecentIssues                   // LightGreen
	PersistentIssuesPresent           // Yellow
	ShortlyActiveIssues               // Orange
	NonEphemeralActiveIssues          // Red
	CheckIsHanging                    // Black
)

type ColorsType struct {
	Background string
	Border     string
	Text       string // default is white
}

var Colors = map[WatchCon]ColorsType{
	AllOK: {
		consts.Colors["green"],
		consts.Colors["darkgreen"],
		consts.Colors["white"],
	},
	InitialCheck: {
		consts.Colors["white"],
		consts.Colors["gray"],
		consts.Colors["black"],
	},
	HadRecentIssues: {
		consts.Colors["blue"],
		consts.Colors["darkblue"],
		consts.Colors["white"],
	},
	PersistentIssuesPresent: {
		consts.Colors["yellow"],
		consts.Colors["orange"],
		consts.Colors["black"],
	},
	ShortlyActiveIssues: {
		consts.Colors["orange"],
		consts.Colors["red"],
		consts.Colors["white"],
	},
	NonEphemeralActiveIssues: {
		consts.Colors["red"],
		consts.Colors["darkred"],
		consts.Colors["white"],
	},
	CheckIsHanging: {
		consts.Colors["black"],
		consts.Colors["gray"],
		consts.Colors["white"],
	},
}

func (s *StatsType) Condition() WatchCon {
	if s.CheckHangs() {
		return CheckIsHanging
	}
	if s.ActiveNonEphemeral > 0 {
		return NonEphemeralActiveIssues
	}
	if s.Active > 0 {
		return ShortlyActiveIssues
	}
	if s.Persistent > 0 {
		return PersistentIssuesPresent
	}
	if s.OnProbation > 0 {
		return HadRecentIssues
	}
	if s.InitialCheck {
		return InitialCheck
	}
	return AllOK
}
