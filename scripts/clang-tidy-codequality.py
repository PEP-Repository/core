#!/usr/bin/env python3

# Converts the findings that clang-tidy exported (with --export-fixes, one YAML file per source file) to a GitLab Code
# Quality report, which GitLab shows on the MR page (see https://docs.gitlab.com/ci/testing/code_quality/).
#
# Usage: clang-tidy-codequality.py FIXES_DIR SOURCE_ROOT > gl-code-quality-report.json
# Writes an empty report if FIXES_DIR doesn't exist (e.g. when the linter was not enabled).

import bisect
import hashlib
import json
import sys
from pathlib import Path

import yaml

SEVERITIES = {"Error": "major", "Warning": "minor", "Remark": "info"}

fixes_dir, source_root = Path(sys.argv[1]), Path(sys.argv[2]).resolve()

content_cache: dict[Path, bytes] = {}
line_starts_cache: dict[Path, list[int]] = {}


def content(path: Path) -> bytes:
  if path not in content_cache:
    content_cache[path] = path.read_bytes() if path.is_file() else b""
  return content_cache[path]


def line_number(path: Path, offset: int) -> int:
  """1-based line number of the byte offset in the file."""
  if path not in line_starts_cache:
    line_starts_cache[path] = [0] + [i + 1 for i, byte in enumerate(content(path)) if byte == ord("\n")]
  # Number of line starts at or before the offset
  return bisect.bisect_right(line_starts_cache[path], offset)


def shorten(text: str, max_length: int = 80) -> str:
  """Text on a single line, truncated to max_length."""
  text = " ".join(text.split())
  return text if len(text) <= max_length else text[:max_length - 1] + "…"


def describe_fix(replacements: list[dict]) -> str:
  """The automatic fix that clang-tidy suggests, e.g. "add `#include <string>`"."""
  parts = []
  for replacement in replacements:
    offset, length = replacement.get("Offset", 0), replacement.get("Length", 0)
    old = content(Path(replacement["FilePath"]).resolve())[offset:offset + length].decode(errors="replace")
    old, new = shorten(old), shorten(replacement.get("ReplacementText", ""))
    if old and new:
      parts.append(f"replace `{old}` with `{new}`")
    elif old:
      parts.append(f"remove `{old}`")
    elif new:
      parts.append(f"add `{new}`")
  return "; ".join(parts)


findings = {}
skipped = 0
for fixes_file in sorted(fixes_dir.glob("**/*.yaml")) if fixes_dir.is_dir() else []:
  for diagnostic in (yaml.load(fixes_file.read_text(), Loader=getattr(yaml, "CSafeLoader", yaml.SafeLoader)) or {}).get(
      "Diagnostics", []):
    message = diagnostic["DiagnosticMessage"]
    if not message.get("FilePath"):
      skipped += 1
      continue
    path = Path(message["FilePath"]).resolve()
    if not path.is_relative_to(source_root) or not path.is_file():
      skipped += 1
      continue
    relative_path = path.relative_to(source_root).as_posix()
    line = line_number(path, message.get("FileOffset", 0))
    check = diagnostic["DiagnosticName"]
    # Headers are linted once per source file that includes them: identical fingerprints are shown once
    fingerprint = hashlib.sha256(f"{relative_path}:{line}:{check}:{message['Message']}".encode()).hexdigest()
    fix = describe_fix(message.get("Replacements", []))
    findings[fingerprint] = {
      "description": f"{message['Message']} [{check}]" + (f" (fix: {fix})" if fix else ""),
      "check_name": check,
      "fingerprint": fingerprint,
      "severity": SEVERITIES.get(diagnostic.get("Level"), "minor"),
      "location": {"path": relative_path, "lines": {"begin": line}},
    }

json.dump(list(findings.values()), sys.stdout, indent=2)
print(f"{len(findings)} clang-tidy findings in Code Quality report"
      + (f", skipped {skipped} outside {source_root}" if skipped else ""), file=sys.stderr)
