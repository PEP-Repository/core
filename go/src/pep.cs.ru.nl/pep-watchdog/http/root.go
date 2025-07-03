package http

import (
	"html/template"
	"log"
	"net/http"
	"time"

	"pep.cs.ru.nl/pep-watchdog/consts"
	"pep.cs.ru.nl/pep-watchdog/shared"
	"pep.cs.ru.nl/pep-watchdog/state"

	"github.com/dustin/go-humanize"
)

var parsedTemplate *template.Template

var rawTemplate = `
<html>
    <head>
        <title>PEP {{ .Name }} watchdog</title>
        <style>
        body {
            color: {{ .Colors.Text }};
            background-color: {{ .Colors.Background }};
            font-family: Open Sans,Helvetica,Arial,sans-serif;
            font-size: smaller;
        }
        </style>
    </head>
    <body>
        {{ if .Report.Stats.CheckInProgress }}
        <p>Check in progress, started 
	    {{ format_time .Report.Stats.CheckStarted }}.</p>
        {{ end }}

        {{ if .Report.Stats.InitialCheck }}
        <p>(Initial check)</p>
	{{ else }}
	<ul>
	  {{ range $i, $issue := .Report.Active }}
	      <li>
	      {{ $issue.Message }}
	      &mdash;
	      <em>First seen {{ format_time $issue.Discovery }}.</em>
	      {{ if not $issue.Reported }}
	      <em>Not yet reported.</em>
	      {{ end }}
	      {{ if not $issue.NonEphemeral }}
	      <em>Possibly ephemeral.</em>
	      {{ end }}
	      </li>
	  {{ else }}
	    <li>No active issues!</li>
	  {{ end }}
	</ul>

	{{ if or .Report.OnProbation }}
	<p>Recent issues:</p>
	<ul>
	  {{ range $i, $issue := .Report.OnProbation }}
	      <li>
	      {{ $issue.Message  }}
	      &mdash;
	      <em>Last seen {{ format_time $issue.LastSighting }};
	      first seen {{ $issue.LastSighting.Sub $issue.Discovery | format_duration }} before that.</em>
	      {{ if not $issue.Reported }}
	      <em>Not yet reported.</em>
	      {{ end }}
	      {{ if not $issue.NonEphemeral }}
	      <em>Ephemeral.</em>
	      {{ end }}
	      </li>
	  {{ end }}
	</ul>
	{{ end }}

	{{ if or .Report.Removed }}
	<p>The following issues have expired and will be removed after being
	reported:</p>
	<ul>
	  {{ range $i, $issue := .Report.Removed }}
	      <li>
	      {{ $issue.Message  }}
	      &mdash;
	      <em>Last seen {{ format_time $issue.LastSighting }};
	      first seen {{ $issue.LastSighting.Sub $issue.Discovery | format_duration }} before that.</em>
	      {{ if not $issue.Reported }}
	      <em>Not yet reported.</em>
	      {{ end }}
	      {{ if not $issue.NonEphemeral }}
	      <em>Ephemeral.</em>
	      {{ end }}
	      </li>
	  {{ end }}
	</ul>
	{{ end }}

	{{ if .Report.Persistent }}
	<p>Persistent issues:</p>
	<ul>
	  {{ range $i, $pi := .Report.Persistent }}
	  <li> {{ .Message }} &mdash;
	  <em>Occurred {{ format_time_complete .At }}.</em>
	  </li>
	  {{ end }}
	</ul>
	{{ end }}
        <p>Last update {{ format_time .Report.Stats.LastCheck }}</p>
        {{ end }}
	<p>{{ .Name }} watchdog</p>
        <script type="text/javascript">
            setTimeout(function() {
                window.location.reload(1);
            }, {{ .Interval }});
        </script>
    </body>
</html>
`

type templateContext struct {
	// To prevent data races, make sure the template context
	// contains only copies of current state
	Name     string
	Report   *state.Report
	Interval int
	Colors   state.ColorsType
}

func setupRootHandler() {
	var err error
	parsedTemplate, err = template.New("template").Funcs(template.FuncMap{
		"format_time": func(at time.Time) string {
			return humanize.Time(at)
		},
		"format_time_complete": func(at time.Time) string {
			return humanize.Time(at) +
				" (" + at.Format(consts.TimeFormat) + ")"
		},
		"format_duration": func(d time.Duration) string {
			return d.Truncate(time.Second).String()
		},
	}).Parse(rawTemplate)

	if err != nil {
		panic(err)
	}
}

// Handles HTTP GET on /
func rootHandler(w http.ResponseWriter, r *http.Request) {

	err := parsedTemplate.Execute(w,
		func() *templateContext {
			// only lock stateMux to create the template context
			state.Mux.RLock()
			defer state.Mux.RUnlock()

			report := state.CompileReport(false)

			return &templateContext{
				Name:     shared.Conf.Name,
				Report:   report,
				Interval: 20000,
				Colors:   state.Colors[report.Stats.Condition()],
			}
		}(),
	)
	if err != nil {
		log.Printf("Error executing template: %s", err)
	}
}
