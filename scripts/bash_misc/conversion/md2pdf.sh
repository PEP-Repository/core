#!/usr/bin/env bash

# pandoc is used to generate presentation PDF (via latex documentclass beamer) files from MD files. This script sets some desired parameters.
# Instead of '.pdf', other output formats supported by pandoc may be selected, e.g. .tex .

# Install mermaid filter using: npm i -g mermaid-filter # source https://gist.github.com/letientai299/2c974b4f5e7b05be52d369ff8693c29a

# When mermaid-filter producess puppeteer errors related to sandboxing, use the -P flag with mmdc precompilation:
# Required packages: 
#  mmdc/mermaild-cli: npm i -g @mermaid-js/mermaid-cli
# rsvg-convert: package `librsvg2-bin` at debian-based

# Automatically calls script when source has altered.
WATCH_SOURCE=0
# When output is set to beamer, the header-level (from .md or resulting LaTeX) defines what are slide (sub)titles etc.
SLIDELEVEL=2
# When PRE_SVG_MD is set "1" (-P flag) it uses mmdc to precompile mermaid in md to externally linked svg files in an md
PRE_SVG_MD=0
# Set to a landscape parameter if the output pdf should have this orientation.
PAGE_GEOMETRY=""
# Allows various types of output (beamer flag e.g.)
PDF_TYPE="default"

##########################################################################################
############################ Bash script argument parsing ################################
##########################################################################################
function print_cmd_usage { # cli overview of parameters
  cat << EOF
  Usage: md2pdf.sh <switches> mdSource [pdfDestination]
  
  Switches: 
    --beamer/-b
    --help/-h                       This parameter overview.
    --preSvgMd/-P                   Precompile .md file with mermaid json to .md file with svg references.
    --slide-level/-s                The amount of #'s for a new slide when using the --beamer flag (default: 2).
    --landscape/-L                  Change document page geometry (orientation) to landscape.
    --watch/-w                      Watch mdSource and rebuild beamerpdf on modification (does not always function correctly).

  Note: try the -P flag if mermaid diagrams do not build. If text in diagrams does not show, try adding the the code '%%{init: {"flowchart": { "htmlLabels": false}} }%%' directly after an opening '\`\`\`mermaid'

  .
EOF

}
# Substitute long options to short ones (thnx https://stackoverflow.com/a/30026641/3906500 )
for arg in "$@"; do
  shift
  case "$arg" in
    '--beamer')           set -- "$@" '-b'   ;;
    '--landscape')        set -- "$@" '-L'   ;;
    '--watch')            set -- "$@" '-w'   ;;
    '--help')             set -- "$@" '-h'   ;;
    '--slide-level')      set -- "$@" '-s'   ;;
    *)                    set -- "$@" "$arg" ;;
  esac
done
while getopts 'bwhLPs:' flag; do
  case "${flag}" in
    b)      # Use beamer pandoc settings
            PDF_TYPE="beamer"
            ;;
    w)      # Set WATCH_SOURCE flag (default 0)
            WATCH_SOURCE=1
            ;;
    h)      # show help message
            print_cmd_usage
            exit 2
            ;;
    L)      # Change geometry of output PDF to landscape
            PAGE_GEOMETRY='-V geometry:landscape'
            ;;
    P)      # Precompile .md file with mermaid json to .md file with svg references.
            # inspired by https://github.com/mermaid-js/mermaid-cli/blob/master/docs/linux-sandbox-issue.md
            if [ ! -f "puppeteer-config-nosandbox.json" ]; then
              printf "{\n\"args\": [\"--no-sandbox\"]\n}" > puppeteer-config-nosandbox.json
            fi
            PRE_SVG_MD=1
            ;;
    s)      # set slide level for beamer markdown (use subsections ## for pages)
            SLIDELEVEL="$OPTARG"
            ;;
    ?)
            console_alert "Unknown parameter in $*."
            print_cmd_usage
            exit 2
            ;;
  esac
done # getopts
shift $((OPTIND-1))
############################ End of Bash script argument parsing ################################

SOURCE_INFILE="$1"
INFILE="$SOURCE_INFILE"

if [ -z "$2" ]; then
  OUTFILE="$INFILE.pdf"
else
  OUTFILE="$2"
fi

function preprocess_mermaid {
  echo "Pre-processing .md to convert inline mermaid to external svg files."
  TOUTFILE="$SOURCE_INFILE-mermaidsvg.md"
  mmdc -p puppeteer-config-nosandbox.json -i "$SOURCE_INFILE" -o "$TOUTFILE"
  INFILE="$TOUTFILE"
}


function build_pdf {
  if [[ "$PRE_SVG_MD" == "1" ]]; then
    preprocess_mermaid
  fi

  echo "Building $INFILE to $OUTFILE"

  case $PDF_TYPE in
    "default")
      pandoc -s --pdf-engine=xelatex -V mainfont="Liberation Serif" $PAGE_GEOMETRY -f markdown-implicit_figures -t pdf -V colorlinks=true -V linkcolor=blue -V urlcolor=red -V toccolor=gray "$INFILE" -o "$OUTFILE"
      ;;
    "beamer")
      pandoc -s --pdf-engine=xelatex -V mainfont="Liberation Serif" -f markdown --slide-level "$SLIDELEVEL" -F mermaid-filter -t beamer "$INFILE" -o "$OUTFILE" # Do show image captions
      ;;
    *)
      echo "Unknown PDF_TYPE: $PDF_TYPE."
      ;;
  esac
}

if [[ "$WATCH_SOURCE" == "1" ]]; then
  echo "Watching \"$INFILE\" in while loop (should build again upon file change)"
  while fsnotifywait -e modify "$INFILE"; do
      echo "(re)building watched file: $INFILE "
      build_pdf
  done

  echo "End of watch cycle"

else # if not WATCH_SOURCE == 1
  build_pdf
fi
