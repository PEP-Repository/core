package check

import "context"

type Context interface {
	context.Context

	Issuef(string, ...interface{})
	PersistentIssuef(string, ...interface{})
}

type Func func(Context)
