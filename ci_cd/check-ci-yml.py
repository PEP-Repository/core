#!/usr/bin/env python3

# Checks pep/core's CI configuration (.gitlab-ci.yml and the files it includes) for rules we want to enforce.
#
# To add a rule: write a function that takes the configuration and returns a list of problems (empty if all is
# well), and add it to CHECKS. Its docstring is printed as the explanation when the check fails.


import sys
from pathlib import Path

import yaml

# Top-level keys that are not jobs, see https://docs.gitlab.com/ci/yaml/#global-keywords
GLOBAL_KEYWORDS = {"after_script", "before_script", "cache", "default", "image", "include", "services", "stages",
                   "variables", "workflow"}

# `!reference [job, key]` is resolved by GitLab; here it only needs to parse
yaml.SafeLoader.add_constructor("!reference", yaml.SafeLoader.construct_sequence)


def load(path: str) -> dict:
  """Top-level keys of path and the (local) files it includes, with path's own keys overriding included ones."""
  # A file may start with a `spec:` document, followed by `---` and the actual configuration
  config = list(yaml.safe_load_all(Path(path.lstrip("/")).read_text()))[-1]
  includes = config.get("include", [])
  merged = {}
  for include in includes if isinstance(includes, list) else [includes]:
    merged |= load(include if isinstance(include, str) else include["local"])
  return merged | config


def jobs(config: dict) -> list[str]:
  """Jobs that end up in pipelines: not global keywords, and not hidden (starting with `.`)."""
  return [key for key in config if key not in GLOBAL_KEYWORDS and not key.startswith(".")]


def has(config: dict, job: str, key: str) -> bool:
  """Whether the job has key, either itself or through `extends:`."""
  parents = config[job].get("extends", [])
  return key in config[job] or any(has(config, parent, key)
                                   for parent in ([parents] if isinstance(parents, str) else parents))


def every_job_has_rules(config: dict) -> list[str]:
  """Jobs without `rules:` default to `only: [branches, tags]`, so they are silently left out of MR pipelines
  (see https://docs.gitlab.com/ci/yaml/deprecated_keywords/#onlyrefs--exceptrefs).
  Add `rules:` (e.g. `rules: *run-in-every-pipeline`), or extend a job that has them."""
  return [f"{job} has no `rules:`" for job in jobs(config) if not has(config, job, "rules")]


CHECKS = [every_job_has_rules]

config = load(".gitlab-ci.yml")
failed = False
for check in CHECKS:
  problems = check(config)
  failed |= bool(problems)
  if problems:
    print(f"FAILED: {check.__name__}", *(f"  {problem}" for problem in problems), check.__doc__, sep="\n",
          file=sys.stderr)
  else:
    print(f"OK: {check.__name__}")
sys.exit(failed)
