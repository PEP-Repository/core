package mail

import (
	"fmt"
	"log"
	"strconv"
	"time"

	"github.com/dustin/go-humanize"
	"gopkg.in/gomail.v2"

	"pep.cs.ru.nl/pep-watchdog/consts"
	"pep.cs.ru.nl/pep-watchdog/shared"
	"pep.cs.ru.nl/pep-watchdog/state"
)

var (
	mailer *gomail.Dialer
)

func Setup() {
	if shared.Conf.Mailer.HasValue() {
		mailer = gomail.NewDialer(
			shared.Conf.Mailer.Host,
			shared.Conf.Mailer.Port,
			shared.Conf.Mailer.Username,
			shared.Conf.Mailer.Password)
	}
}

func prepareMessage() *gomail.Message {
	m := gomail.NewMessage()
	m.SetHeader("From", shared.Conf.Mailer.From)
	m.SetHeader("To", shared.Conf.Mailer.To)
	return m
}

// Send an e-mail on changes to the list of issues
func Issues(report *state.Report) {
	if mailer == nil {
		log.Printf("Would have send an issue report now by mail" +
			" if a mailer had been configured.")
		return
	}

	m := prepareMessage()

	m.SetHeader("Subject", fmt.Sprintf(
		"%d active issues, %d persistent",
		len(report.Active), len(report.Persistent)))
	msg := ""

	if len(report.Persistent) > 0 {
		msg += "\n\nPersistent issues:\n\n"
		for _, pi := range report.Persistent {
			msg += "  * " + strconv.Quote(pi.Message) + ", since " +
				humanize.Time(pi.At) + ".\n"
		}
	}

	printIssue := func(issue state.Issue) {
		msg += "  * " + strconv.Quote(issue.Message) + ", "
		if !issue.Reported() {
			msg += "*new*, "
		}
		if !issue.NonEphemeral() {
			msg += "possibly ephemeral, "
		}
		msg += "discovered " +
			humanize.Time(issue.Discovery) + ", "
	}

	if len(report.Active) > 0 {
		msg += "\n\nThe following issue(s) are active:\n\n"
		for _, issue := range report.Active {
			printIssue(issue)
			msg += "\n"
		}
	}

	if len(report.OnProbation) > 0 {
		msg += "\n\nThe following issue(s)" +
			" have been active recently:\n\n"
		for _, issue := range report.OnProbation {
			printIssue(issue)
			msg += "last seen " +
				humanize.Time(issue.LastSighting) + ", "
			msg += "\n"
		}
	}

	if len(report.Removed) > 0 {
		msg += "\n\nThe following issue(s)" +
			" have expired and have been " +
			"removed from our records:\n\n"
		for _, issue := range report.Removed {
			printIssue(issue)
			msg += "last seen " +
				humanize.Time(issue.LastSighting) + ", "
			msg += "\n"
		}
	}

	msg += fmt.Sprintf("\n\nSee %s for more details.\n\n",
		*shared.Conf.Url)
	msg += "   PEP " + shared.Conf.Name + " Watchdog"
	m.SetBody("text/plain", msg)

	if err := mailer.DialAndSend(m); err != nil {
		log.Printf("Failed to send e-mail: %s", err)
	}
}

func Test() {
	if mailer == nil {
		log.Fatalf("Error: no mailer has been configured in %v",
			shared.Args.ConfPath)
	}

	m := prepareMessage()

	m.SetHeader("Subject",
		fmt.Sprintf("Test email from %v Watchdog", shared.Conf.Name))
	m.SetBody("text/plain",
		fmt.Sprintf("Send %v", time.Now().Format(consts.TimeFormat)))

	if err := mailer.DialAndSend(m); err != nil {
		log.Fatalf("Failed to send test e-mail: %s", err)
	}
}
