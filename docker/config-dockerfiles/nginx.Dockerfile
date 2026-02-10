FROM nginx
ARG ENVIRONMENT=main

COPY ${ENVIRONMENT}/nginx/etc /etc/nginx
COPY ${ENVIRONMENT}/nginx/www /srv/www