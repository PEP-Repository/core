package http

import (
	"bytes"
	"fmt"
	"net/http"
	"strconv"
	"sync"
	"time"

	"github.com/fogleman/gg"
	"github.com/golang/freetype/truetype"
	"golang.org/x/image/font"
	"golang.org/x/image/font/gofont/goregular"

	"pep.cs.ru.nl/pep-watchdog/shared"
	"pep.cs.ru.nl/pep-watchdog/state"
)

var (
	badge         []byte
	badgeLock     sync.Mutex
	badgeRendered time.Time
)

func loadFont(points float64) (font.Face, error) {
	f, err := truetype.Parse(goregular.TTF)
	if err != nil {
		return nil, err
	}
	return truetype.NewFace(f, &truetype.Options{Size: points}), nil
}

func ensureBadgeIsRendered() ([]byte, time.Time) {
	badgeLock.Lock()
	defer badgeLock.Unlock()

	var stats state.StatsType

	func() {
		state.Mux.RLock()
		defer state.Mux.RUnlock()

		stats = state.Stats
	}()

	if badgeRendered.After(stats.LastChange) && !stats.CheckInProgress {
		return badge, badgeRendered
	}

	nIssues := stats.Persistent + stats.Active

	colors := state.Colors[stats.Condition()]

	msg := "ok"
	if nIssues > 0 {
		msg = fmt.Sprintf("%d", nIssues)
	}
	if stats.CheckInProgress {
		msg += "?"
	}

	width := 600.
	height := width / 5.
	margin := width / 100.
	c := gg.NewContext(int(width), int(height))
	c.DrawRoundedRectangle(
		margin,
		margin,
		width-2*margin,
		height-2*margin,
		width/60.)
	c.SetHexColor(colors.Background)
	c.FillPreserve()
	c.SetHexColor(colors.Border)
	c.SetLineWidth(width / 100.)
	c.Stroke()

	c.DrawRoundedRectangle(
		2*margin,
		2*margin,
		width/4*3-4*margin,
		height-4*margin,
		width/120)
	c.SetHexColor("#333")
	c.FillPreserve()
	c.SetHexColor("#777")
	c.SetLineWidth(width / 300.)
	c.Stroke()

	var name = shared.Conf.Name
	if name == "" {
		name = "Watchdog"
	}
	fontFace, _ := loadFont(width / 8.)
	c.SetFontFace(fontFace)
	c.SetHexColor("#fff")
	c.DrawString(name+":", margin*4, height-margin*6)

	c.SetHexColor(colors.Text)
	c.DrawString(msg, width/4*3+3*margin, height-margin*6)

	var buf bytes.Buffer
	c.EncodePNG(&buf)
	badge = buf.Bytes()
	badgeRendered = time.Now()
	return badge, badgeRendered
}

func badgeHandler(w http.ResponseWriter, r *http.Request) {
	badge, at := ensureBadgeIsRendered()
	w.Header().Set("Content-Type", "image/png")
	w.Header().Set("Etag", at.String())
	w.Header().Set("Content-Length", strconv.Itoa(len(badge)))
	w.Write(badge)
}
