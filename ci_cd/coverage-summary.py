#!/usr/bin/env python3

# Combines the Cobertura coverage reports of the build jobs into one line coverage percentage.
#
# GitLab's `coverage:` keyword takes a single number per job, so it cannot combine the percentages of the jobs that
# each measure part of the code. Adding up their line counts keeps the total weighted by the amount of code each
# job reports on, which averaging the percentages would not.


import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

# Written by the jobs that measure coverage, and collected here through `needs:`
REPORT_PATTERN = "coverage*.xml"

TOTAL_PATH = Path("coverage-total.txt")


def line_counts(report: Path) -> tuple[int, int]:
  """The number of covered and of instrumented ("valid") lines in a Cobertura report."""
  attributes = ElementTree.parse(report).getroot().attrib
  try:
    return int(attributes["lines-covered"]), int(attributes["lines-valid"])
  except KeyError as missing:
    sys.exit(f"{report} has no {missing} attribute: did the tool that writes it change?")


def percentage(covered: int, valid: int) -> str:
  return f"{100 * covered / valid:.2f}"


reports = sorted(Path().glob(REPORT_PATTERN))
if not reports:
  sys.exit(f"No coverage reports ({REPORT_PATTERN}) found. The jobs that produce them should run with "
           "ENABLE_COVERAGE=ON and pass their report on as an artifact.")

total_covered = total_valid = 0
for report in reports:
  covered, valid = line_counts(report)
  if valid == 0:
    sys.exit(f"{report} reports on no lines at all.")
  print(f"{report}: {covered} of {valid} lines covered ({percentage(covered, valid)}%)")
  total_covered += covered
  total_valid += valid

total = percentage(total_covered, total_valid)
TOTAL_PATH.write_text(total)

# Picked up by the `coverage:` regex of the job that runs this script
print(f"Line Coverage: {total}%")
