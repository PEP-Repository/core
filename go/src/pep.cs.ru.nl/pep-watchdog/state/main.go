package state

import (
	"sync"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

var (
	Stats  = StatsType{InitialCheck: true}
	Issues = make(map[string]*IssueMeta)
	Mux    sync.RWMutex
)

// additional info kept in memory on this non-persistent issue, for reporting
type IssueMeta struct {
	Active       bool // present at last check
	Discovery    time.Time
	LastSighting time.Time // always after Discovery

	// When the first email about this issue was send.
	// Zero when not reported, otherwise after Discovery.
	FirstReportedAt time.Time
}

func NewIssueMeta(now time.Time) *IssueMeta {
	return &IssueMeta{
		Active:       true,
		Discovery:    now,
		LastSighting: now,
	}
}

func (r IssueMeta) NonEphemeral() bool {
	return r.LastSighting.After(r.Discovery.Add(shared.Conf.EphemeralBelow))
}

func (r IssueMeta) Reported() bool {
	return !r.FirstReportedAt.IsZero()
}

func (r IssueMeta) OnProbation(now time.Time) bool {
	return !r.Active && now.Sub(r.LastSighting) <=
		shared.Conf.ProbationPeriod
}

type StatsType struct {
	IssueStats

	InitialCheck    bool
	CheckInProgress bool
	CheckStarted    time.Time
	LastCheck       time.Time
	LastChange      time.Time
}

type IssueStats struct {
	UnreportedSolved             uint
	UnreportedActiveNonEphemeral uint
	Active                       uint
	ActiveNonEphemeral           uint
	OnProbation                  uint
	Persistent                   uint
}

func (s *IssueStats) MustReport() bool {
	return s.UnreportedActiveNonEphemeral > 0 ||
		s.UnreportedSolved > shared.Conf.MaxUnreportedSolvedIssues
}

func (s *StatsType) CheckHangs() bool {
	return time.Now().After(s.CheckStarted.Add(shared.Conf.Interval))
}

// Mux write lock must be held
func SetIssueStats() {
	now := time.Now()

	var res IssueStats

	res.Persistent = (uint)(len(GetPersistentIssues()))

	for _, issue := range Issues {
		if issue.Active {
			res.Active += 1
			if issue.NonEphemeral() {
				res.ActiveNonEphemeral += 1
				if !issue.Reported() {
					res.UnreportedActiveNonEphemeral += 1
				}
			}

		} else {
			if !issue.Reported() {
				res.UnreportedSolved += 1
			}
			if issue.OnProbation(now) {
				res.OnProbation += 1
			}
		}
	}

	Stats.IssueStats = res
}
