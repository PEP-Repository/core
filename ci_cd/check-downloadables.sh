#!/usr/bin/env sh

set -eu

SLEEP_DURATION="30s"

# The following does not work for filenames which contain newlines. We would need bash instead of sh to solve. See https://www.shellcheck.net/wiki/SC2044
# But we have full control over what we put in the publish directory, and can therefore be sure that there will be no newlines
find publish -type f | while IFS= read -r file; do
  pipeline_modified_timestamp=$(stat -c%Y "$file")
  url=$(echo "https://pep.cs.ru.nl${file#publish}" | sed "s/ /%20/g; s/(/%28/g; s/)/%29/g")
  while :; do
    published_modified_date="$(curl --silent --head "$url" | grep -i "^Last-Modified:" | cut "-d:" -f2-)"
    if [ -z "$published_modified_date" ]; then
      echo "Last-Modified header not set. Maybe the file does not exist yet on the server."
    else
      published_modified_timestamp=$(date --date="$published_modified_date" +%s)
      if [ "$published_modified_timestamp" -ge "$pipeline_modified_timestamp" ]; then
        echo "File published at $url";
        break;
      fi

      echo "The currently published $file is older than the one from current pipeline.";
    fi
    echo "Checking again in $SLEEP_DURATION"
    sleep $SLEEP_DURATION
  done
done