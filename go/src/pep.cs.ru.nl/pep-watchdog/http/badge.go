package http

import (
	"bytes"
	"fmt"
	"net/http"
	"os"
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

const (
	nameAreaRatio    = 0.75
	minFontSize      = 8.
	maxFontSizeRatio = 0.8
	badgeWidth       = 600.
	badgeHeight      = badgeWidth / 5.
	outerBadgeStroke = badgeWidth / 100.
	maxFontSize      = badgeHeight * maxFontSizeRatio
	badgeRoundover   = badgeWidth / 60.

	nameAreaWidth          = badgeWidth * nameAreaRatio
	nameBoxStroke          = outerBadgeStroke / 3.
	nameBoxMargin          = outerBadgeStroke * 0.5
	nameBoxRoundover       = badgeRoundover / 2.
	nameBoxBackgroundColor = "#333"
	nameBoxStrokeColor     = "#777"
	nameBoxTextColor       = "#fff"

	statusAreaWidth = badgeWidth * (1. - nameAreaRatio)
)

func loadFont(points float64) (font.Face, error) {
	f, err := truetype.Parse(goregular.TTF)
	if err != nil {
		return nil, err
	}
	return truetype.NewFace(f, &truetype.Options{Size: points}), nil
}

func drawErrorIndicator(ctx *gg.Context, centerX, centerY, size float64) {
	radius := size / 2

	// Draw circle with red fill and white border
	ctx.DrawCircle(centerX, centerY, radius)
	ctx.SetHexColor("#ff0000")
	ctx.FillPreserve()
	ctx.SetHexColor("#ffffff")
	ctx.SetLineWidth(outerBadgeStroke)
	ctx.Stroke()

	// Draw white X pattern inside the circle, with slightly smaller size so that it fits snugly
	insetRadius := radius * 0.6
	ctx.DrawLine(centerX-insetRadius, centerY-insetRadius, centerX+insetRadius, centerY+insetRadius)
	ctx.DrawLine(centerX+insetRadius, centerY-insetRadius, centerX-insetRadius, centerY+insetRadius)
	ctx.Stroke()
}

// setFontFaceFor calculates font size to fit text in given dimensions and sets it on the context
func setFontFaceFor(text string, boxWidth, boxHeight float64, ctx *gg.Context) error {
	// Start with font size proportional to box height
	fontSize := maxFontSize
	if fontSize < minFontSize {
		fontSize = minFontSize
	}

	face, err := loadFont(fontSize)
	// Fallback to minimum font size if loading fails
	if err != nil {
		face, _ = loadFont(minFontSize)
		if face == nil {
			return fmt.Errorf("Failed to load font for badge: %w", err)
		}
		fontSize = minFontSize
	}

	ctx.SetFontFace(face)

	// String height is unnecessary for text
	textWidth, _ := ctx.MeasureString(text)

	// Scale down text, with min font size
	if textWidth > boxWidth && textWidth > 0 {
		fontSize *= (boxWidth / textWidth)
		if fontSize < minFontSize {
			fontSize = minFontSize
		}
		face, err = loadFont(fontSize)
		if err != nil {
			face, _ = loadFont(minFontSize)
			if face == nil {
				return fmt.Errorf("Failed to load font: %w", err)
			}
		}
		ctx.SetFontFace(face)
	}

	return nil
}

func ensureBadgeIsRendered() ([]byte, time.Time) {
	badgeLock.Lock()
	defer badgeLock.Unlock()

	var stats state.StatsType
	confName := shared.Conf.Name

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

	// Prepare name string
	nameText := confName
	if nameText == "" {
		nameText = "Watchdog"
	}
	nameString := nameText + ":"

	// Prepare status message string
	msgString := "ok"
	if nIssues > 0 {
		msgString = fmt.Sprintf("%d", nIssues)
	}
	if stats.CheckInProgress {
		msgString += "?"
	}

	ctx := gg.NewContext(int(badgeWidth), int(badgeHeight))

	// --- Drawing ---

	// Outer rounded rectangle (badge background)
	// Start at half the stroke to correct for badge stroke
	ctx.DrawRoundedRectangle(0.5*outerBadgeStroke, 0.5*outerBadgeStroke, badgeWidth-outerBadgeStroke, badgeHeight-outerBadgeStroke, badgeRoundover)
	ctx.SetHexColor(colors.Background)
	ctx.FillPreserve()
	ctx.SetHexColor(colors.Border)
	ctx.SetLineWidth(outerBadgeStroke)
	ctx.Stroke()

	// Name part background (darker grey box)
	nameBoxWidth := nameAreaWidth - 2.*outerBadgeStroke - 2.*nameBoxMargin
	nameBoxHeight := badgeHeight - 2.*outerBadgeStroke - 2.*nameBoxMargin

	ctx.DrawRoundedRectangle(outerBadgeStroke+nameBoxMargin, outerBadgeStroke+nameBoxMargin, nameBoxWidth, nameBoxHeight, nameBoxRoundover)
	ctx.SetHexColor(nameBoxBackgroundColor)
	ctx.FillPreserve()
	ctx.SetHexColor(nameBoxStrokeColor)
	ctx.SetLineWidth(nameBoxStroke)
	ctx.Stroke()

	centerY := badgeHeight / 2.

	// Draw Name Text (centered in its box)
	centerX := 1.5*outerBadgeStroke + nameBoxWidth/2.
	err := setFontFaceFor(nameString, nameBoxWidth-nameBoxStroke, nameBoxHeight-nameBoxStroke, ctx)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to set font for badge name text: %v\n", err)
		drawErrorIndicator(ctx, centerX, centerY, maxFontSize)
	} else {
		ctx.SetHexColor(nameBoxTextColor)
		ctx.DrawStringAnchored(nameString, centerX, centerY, 0.5, 0.5)
	}

	// Draw Status Text (centered in its area)
	centerX = nameAreaRatio*badgeWidth + (statusAreaWidth-outerBadgeStroke)/2.
	err = setFontFaceFor(msgString, statusAreaWidth-outerBadgeStroke, badgeHeight-2.*outerBadgeStroke, ctx)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to set font for badge status text: %v\n", err)
		drawErrorIndicator(ctx, centerX, centerY, maxFontSize)
	} else {
		ctx.SetHexColor(colors.Text)
		ctx.DrawStringAnchored(msgString, centerX, centerY, 0.5, 0.5)
	}

	var buf bytes.Buffer
	ctx.EncodePNG(&buf)
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
