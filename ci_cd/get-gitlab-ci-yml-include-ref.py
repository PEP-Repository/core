#!/usr/bin/env python3

# Produces the "ref" of included Gitlab CI YAML files.

# The "check-ci-yml-include-versions.sh" script ensures that Gitlab CI logic is
# based on a consistent set of file versions. To do so, it needs to determine
# which version of an included YAML file a CI pipeline will be (or has been)
# based on. The version can be specified with the "ref" of the included file:
# see e.g. https://docs.gitlab.com/ee/ci/yaml/#includefile .
# Since there seems to be no convenient/standardized way to parse YAML from
# shell scripts (see e.g. https://stackoverflow.com/q/5014632), we determine the
# "ref" using this Python script.

import os,sys,yaml

# Define a YAML loader that can deal with Gitlab's custom "!reference" tags
# See https://docs.gitlab.com/ee/ci/yaml/yaml_optimization.html#reference-tags

class gitlabLoader(yaml.SafeLoader):
  pass

def construct_reference(loader, node):
  data = loader.construct_sequence(node)
  return data

yaml.add_constructor('!reference', construct_reference, gitlabLoader)


def main():
  args = sys.argv[1:]
  if len(args) != 3:
    print("Usage:", sys.argv[0], "<yaml>", "<project>", "<file>", file=sys.stderr)
    sys.exit(os.EX_USAGE)

  with open(args[0]) as f:
    ml = yaml.load(f, Loader=gitlabLoader)

  project = args[1]
  file = args[2]

  if 'include' in ml:
    for entry in ml['include']:
      if 'project' in entry and 'file' in entry:
        if entry['project'] == project and entry['file'] == file:
          if 'ref' in entry:
            print(entry['ref'])
          else:
            print('HEAD')

if __name__ == "__main__":
  main()
