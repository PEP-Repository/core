#!/usr/bin/env bash

#openssl genrsa -out /data/client/client.key 2048
(cd /data/client || exit; /app/pepEnrollment ClientConfig.json 1 "ewogICAgInN1YiI6ICJhc3Nlc3NvciIsCiAgICAiZ3JvdXAiOiAiUmVzZWFyY2ggQXNzZXNzb3IiLAogICAgImlhdCI6ICIxNTQyODk1ODg3IiwKICAgICJleHAiOiAiMjA3MzY1NDEyMiIKfQo.cNoT3VMtEZkrHGLOayqj3gwaM7R2BYv24FJpshecK4s" /data/client/ClientKeys.json)
cat /data/client/ClientKeys.json
