FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get upgrade -y --autoremove --purge \
  && apt-get install -y --no-install-recommends apache2 libapache2-mod-shib libapache2-mod-auth-openidc ca-certificates cron curl xmlstarlet \
  && apt-get clean \
  && rm -rf /var/cache/* /var/lib/{apt,dpkg,cache,log}/* /tmp/* /var/tmp/* \
  && rm -rf /etc/apache2/conf-enabled/* /etc/apache2/conf-available/* /etc/apache2/sites-enabled/* /etc/apache2/sites-available/* \
  && mkdir -p /var/run/shibboleth

COPY run.sh download-surfconext-metadata.sh /usr/local/bin/

COPY etc/ /etc/
COPY var/ /var/

RUN a2enmod proxy proxy_http ssl headers include

ENV DEBIAN_FRONTEND=''

EXPOSE 80 443
VOLUME ["/metadata"]

CMD ["run.sh"]
