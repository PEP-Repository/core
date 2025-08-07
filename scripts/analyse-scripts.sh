#!/usr/bin/env bash

set -eou pipefail

exitcode=0

# Check all files with shebang headers
for FILE in $(git --no-pager grep -l -I -E '^#\!'); do
    # Skip files that do not have a shebang at the start of the first line
    if ! [[ $(head -n 1 $FILE) =~ ^"#!" ]]; then
        continue;
    fi

    # Check for executable bit
    if ! [[ -x "$FILE" ]]; then
        >&2 echo "Script '$FILE' does not have executable bit set. Please make it executable by running"
        >&2 echo "    git update-index --chmod=+x \"$FILE\""
        exitcode=1;
    fi

    # Check that shebang header uses /usr/bin/env
    if ! head -n1 "$FILE" | grep -q "^#\!/usr/bin/env"; then
        >&2 echo "Script '$FILE' has shebang header without /usr/bin/env. Pleaes change it to (something like)"
        cmd=$(head -n1 "$FILE" | sed -n "/^#\!/s///p" | xargs basename)
        >&2 echo "    #!/usr/bin/env $cmd"
        exitcode=1;
    fi
done

# Run shellcheck on all .sh files
for FILE in $(git ls-files | grep "\.sh$"); do
    if ! shellcheck "$FILE" --external-sources --severity=warning; then
        exitcode=1
    fi
done

exit $exitcode
