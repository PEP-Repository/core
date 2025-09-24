package state

import (
	"sort"
	"time"
)

type Report struct {
	Persistent []PersistentIssueRecord

	Active      []Issue
	OnProbation []Issue
	Removed     []Issue

	Stats StatsType
}

type Issue struct {
	IssueMeta
	Message string
}

// We sort issues first on most recent sighting,
// then on most recent discovery, and finally on the message.
func sortIssues(issues []Issue) {
	sort.Slice(issues, func(i, j int) bool {
		if issues[i].LastSighting.After(issues[j].LastSighting) {
			return true
		}
		if !issues[i].LastSighting.Equal(issues[j].LastSighting) {
			return false
		}
		if issues[i].Discovery.After(issues[j].Discovery) {
			return true
		}
		if !issues[i].Discovery.Equal(issues[j].Discovery) {
			return false
		}
		return issues[i].Message < issues[j].Message
	})

}

// Collect information on issues.
//
// If official is set, a unique lock on Mux must be held;
// else a read lock on Mux suffices.
func CompileReport(official bool) *Report {
	var res Report

	res.Stats = Stats

	res.Persistent = GetPersistentIssues()
	now := time.Now()

	for msg, meta := range Issues {
		item := Issue{*meta, msg}

		if item.Active {
			res.Active = append(res.Active, item)
		} else if item.OnProbation(now) {
			res.OnProbation = append(res.OnProbation, item)
		} else {
			res.Removed = append(res.Removed, item)
		}

		// update meta
		if official && !meta.Reported() {
			meta.FirstReportedAt = now
		}
	}

	sortIssues(res.Active)
	sortIssues(res.OnProbation)
	sortIssues(res.Removed)

	if official {
		// remove solved issues whose probation has expired
		for _, ipm := range res.Removed {
			delete(Issues, ipm.Message)
		}
	}

	return &res
}
