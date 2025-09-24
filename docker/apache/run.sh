#!/usr/bin/env bash
set -e

rm -f /var/run/httpd/httpd.pid

mkdir -p /secrets_copy
cp -R /secrets/shibboleth /secrets_copy
/bin/chown _shibd:_shibd /secrets_copy/shibboleth/*

spoofKey=$(cat /secrets/spoofKey)
sed -i -e "s/\${SPOOFKEY}/${spoofKey}/" /etc/shibboleth/shibboleth2.xml

# In our repo's, we have files in conf-enabled. This is not what Apache2 expects. The files should be in conf-available,
# and you can enable them by running a2enconf, which creates a symlink in conf-enabled. So we move the files to conf-available and enable them.
# But if there is already a file with the same name in conf-available, it will not be overwritten and enabled. Instead, the file in conf-enabled will be removed.
# This allows project repo's to replace an automatically enabled conf-file with one that is not automatically enabled.
(
  cd /etc/apache2/conf-enabled
  for f in *; do
    if [ ! -f "/etc/apache2/conf-available/$f" ]; then
      mv "$f" /etc/apache2/conf-available/
      a2enconf "$f";
    elif [ ! -L "$f" ]; then # Only remove actual files, not the symlinks created by a2enconf
      rm -f "$f"
    fi
  done
)

/usr/local/bin/download-surfconext-metadata.sh

/usr/sbin/cron

/usr/sbin/shibd -f -F & # -F to run it in the foreground, but also & to run it in the background. This makes sure we see the console output

/usr/local/bin/md-monitor.sh &

/usr/sbin/apache2ctl -DFOREGROUND

sleep 1s # Give some time for the logs to flush
