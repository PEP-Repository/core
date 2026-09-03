#!/usr/bin/env bash

# Checks shell scripts for GNU-only invocations.
#
# Shellcheck covers the shell syntax we write, but not the options we pass to the tools we call.
# Those work on the GNU coreutils of our CI images and fail on the BSD versions that macOS ships,
# so they tend to slip through until someone runs the script on a Mac.
#
# portability-disable-file=all  # this script spells out the very patterns it looks for

set -eou pipefail

usage() {
    echo "Usage: '$0' [--ignore-dir <dir>]... [file...]"
    echo "Checks the given files, or every tracked .sh file, for GNU-only invocations."
    echo "Options:"
    echo "  --ignore-dir <dir>  Skip the scripts under <dir>, which only run on one platform"
    echo "                      and can therefore use that platform's tools freely. Repeatable."
    echo "A check can be switched off by the name it is reported under:"
    echo "  # portability-disable=<name>       switches it off for the line below the comment"
    echo "  # portability-disable-file=<name>  switches it off for the whole file"
    echo "Both directives take \"all\" in place of a check name."
}

ignore_dirs=()
while [ "$#" != 0 ]; do
    case "$1" in
        --ignore-dir)
            shift; ignore_dirs+=("${1:?Expected value for --ignore-dir}") ;;
        --help|-h)
            usage
            exit ;;
        --)
            shift
            break ;;
        --*)
            >&2 echo "$0: Unknown option: $1"
            >&2 usage
            exit 2 ;;
        *) break ;;
    esac
    shift
done

if [ "$#" -gt 0 ]; then
    FILES=("$@")
else
    mapfile -t FILES < <(git ls-files | grep "\.sh$")
fi

if [ "${#ignore_dirs[@]}" -gt 0 ]; then
    kept=()
    for file in "${FILES[@]}"; do
        for dir in "${ignore_dirs[@]}"; do
            if [[ "$file" == "${dir%/}"/* ]]; then
                continue 2
            fi
        done
        kept+=("$file")
    done
    FILES=("${kept[@]}")
fi

# grep would read stdin if we passed it no files at all
if [ "${#FILES[@]}" -eq 0 ]; then
    exit 0
fi

# We report hits by splitting grep's "file:line:text" on its colons, which a colon in a path would
# make ambiguous. Refuse those rather than report a mangled location for them.
for file in "${FILES[@]}"; do
    if [[ "$file" == *:* ]]; then
        >&2 echo "$0: cannot check '$file', because its path contains a colon"
        exit 2
    fi
done

exitcode=0
any_failed=

# Is line $2 of file $1 suppressed for check $3, by a directive for the file or on the line above it?
is_suppressed() {
    local file="$1" line="$2" name="$3" prev=''
    # Line 0 does not exist
    if [ "$line" -gt 1 ]; then
        # print previous line
        prev=$(sed -n "$((line - 1))p" < "$file")
    fi
    # Match the directive anywhere in the file (if it is not followed by - or a letter)
    if grep -qE "portability-disable-file=($name|all)([^-a-z]|$)" -- "$file"; then
        return 0
    # Match the directive anywhere in a comment on the line above (if it is not followed by - or a letter)
    elif [[ "$prev" =~ ^[[:space:]]*#.*portability-disable=($name|all)([^-a-z]|$) ]]; then
        return 0
    fi
    return 1
}

check() {
    local name="$1" pattern="$2" suggestion="$3"
    local hits hit rest file line text reported=

    # -H: print the filename, -n: print the line number
    if ! hits=$(grep -HnE "$pattern" -- "${FILES[@]}"); then
        return 0
    fi

    while IFS= read -r hit; do
        # Split grep -Hn's "file:line:text". The pattern is: ":*"
        file="${hit%%:*}" # "%%" strips the longest match from the back, leaving first part to get the filename
        rest="${hit#*:}" # "#" strips the shortest match, leaving everything after the first colon
        line="${rest%%:*}" # Do the same again to get the line number
        text="${rest#*:}" # The part that's left is the text of the line
        # Whole-line comments are skipped, so a trailing comment that mentions a non portable cmd is still
        # reported. Put a portability-disable directive above such a line.
        if [[ "$text" =~ ^[[:space:]]*# ]]; then
            continue
        elif is_suppressed "$file" "$line" "$name"; then
            continue
        fi
        if [ -z "$reported" ]; then
            >&2 echo "Non-portable: $name"
            reported=1
        fi
        >&2 echo "    $file:$line:$text"
    done <<< "$hits"

    if [ -z "$reported" ]; then
        return 0
    fi
    >&2 echo "  $suggestion"
    any_failed=1
    exitcode=1
}

# Regex fragment matching the start of a command or a non-word character.
cmd_start='(^|[^[:alnum:]_])'

# Regex fragment matching an option/argument boundary.
# Either a non-alphanumeric, underscore or hyphen character, or the end of the line.
word_end='([^[:alnum:]_.-]|$)'

# Regex fragment matching arguments that stay within the current command.
not_cmd_end='[[:space:]][^|;&]*'

# These are some common portability issues, the list is not complete. Please extend with more checks as needed.
# NOTE: Short options combined into a single argument (e.g. -iP, -xzf) are not detected by these checks.
check 'date-d' \
    "${cmd_start}date${not_cmd_end}(-d|--date)${word_end}" \
    'BSD date has no -d, use the gnu_date wrapper by sourcing sh-utils.sh'
check 'sed-i' \
    "${cmd_start}sed${not_cmd_end}-i${word_end}" \
    'GNU sed reads the next argument as its script, BSD sed reads it as the suffix. Write to a temporary file, or pass a suffix as in "sed -i.bak"'
check 'grep-P' \
    "${cmd_start}grep${not_cmd_end}-[[:alpha:]]*P${word_end}" \
    'BSD grep has no -P, use -E and a POSIX ERE instead'
check 'readlink-f' \
    "${cmd_start}readlink${not_cmd_end}-[[:alpha:]]*f${word_end}" \
    'readlink -f is not portable, macOS and gnu utils differ in behavior. Use a portable path canonicalization such as cd + pwd'
check 'stat-c' \
    "${cmd_start}stat${not_cmd_end}-[[:alpha:]]*c${word_end}" \
    'BSD stat uses -f for this instead, with different format specifiers. Use the gnu_stat wrapper by sourcing sh-utils.sh'
check 'head-tail-negative' \
    "${cmd_start}(head|tail)${not_cmd_end}-n[[:space:]]-[[:digit:]]" \
    'A negative count is GNU-only, use a portable alternative'

if [ -n "$any_failed" ]; then
    >&2 echo
    >&2 echo "If one of these options is genuinely needed, do not just disable the check: call the GNU"
    >&2 echo "tool through a wrapper, the way gnu_date does in scripts/sh-utils.sh."
    >&2 echo "e.g. use the g-prefixed build that Homebrew installs (gdate/gstat/greadlink etc.)."
fi

exit $exitcode
