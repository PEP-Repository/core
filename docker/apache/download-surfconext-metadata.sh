#!/usr/bin/env sh

mkdir -p /metadata
curl -o /metadata/surfconext.xml https://metadata.surfconext.nl/idp-metadata.xml
validUntil="$(xmlstarlet sel -t -v '/md:EntityDescriptor/@validUntil' /metadata/surfconext.xml)"
validUntilTimestamp="$(date --date="$validUntil" +%s)"
echo "surfconext_metadata_valid_until $validUntilTimestamp" > /var/www/html/metrics/surfconext-metadata