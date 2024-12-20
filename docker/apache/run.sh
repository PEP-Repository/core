#!/usr/bin/env bash
set -e

rm -f /var/run/httpd/httpd.pid

mkdir -p /secrets_copy
cp -R /secrets/shibboleth /secrets_copy
/bin/chown _shibd:_shibd /secrets_copy/shibboleth/*

spoofKey=$(cat /secrets/spoofKey)
sed -i -e "s/\${SPOOFKEY}/${spoofKey}/" /etc/shibboleth/shibboleth2.xml

# We removed every file in conf-available, before adding the files we actually want to be enabled
# So we can safely enable all of them.
# We could do this in the dockerfile, but if e.g. a config-dockerfile adds new files, they would have to run it again
# Now we have a little more overhead on start, but we're sure all added config is enabled
prev_dir=$(pwd)
cd /etc/apache2/conf-available
for f in ./*; do
    a2enconf "$f";
done
cd "$prev_dir"

/usr/local/bin/download-surfconext-metadata.sh

/usr/sbin/cron

/usr/sbin/shibd -f -F & # -F to run in in the foreground, but also & to run it in de background. This makes sure we see the console output

/usr/sbin/apache2ctl -DFOREGROUND
