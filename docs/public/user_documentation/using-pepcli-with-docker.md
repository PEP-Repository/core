# Using pepcli with Docker

Depending on the environment, you'll need to use different `client` images. The client images for the various environments are published at the following locations ([URI](https://en.wikipedia.org/wiki/Uniform_Resource_Identifier)'s):

| Project | Acceptance URI | Production URI |
| ------- | ---------- | ---------- |
| PPP/POM | `gitlabregistry.pep.cs.ru.nl/pep-public/core/ppp-acc:latest` | `gitlabregistry.pep.cs.ru.nl/pep-public/core/ppp-prod:latest` |
| Healthy Brain | `gitlabregistry.pep.cs.ru.nl/pep-public/core/hb-acc:latest` | `gitlabregistry.pep.cs.ru.nl/pep-public/core/hb-prod:latest` |
| Play | `gitlabregistry.pep.cs.ru.nl/pep-public/core/play-acc:latest` | *none* |

See also: [Tutorial on how to use Docker with Mac OS](docker-macos-tutorial.md)

`pepcli` can be found inside these images in `/app`

 Depending on your usage, you may need to run docker with some bind mounts:

- Any files that you want to be available inside the container (e.g. OAuthToken.json)
- A writable volume to write data to, e.g. when using `pepcli pull`.

For example, if you have your OAuth token in `/home/$(whoami)/PEP/OAuthToken.json`, want to download data to `/mnt/data/pulled-data`, and want to set the config directory to '/config' using the environment variable route, you could run docker as follows:

```shell
docker run -it --volume /mnt/data:/output --volume /home/$(whoami)/PEP:/token:ro -e PEP_CONFIG_DIR='/config' <IMAGE_URI> bash
```

You now have a bash shell inside the docker container. You can do the following to download data:

```shell
cd /output
/app/pepcli --oauth-token /token/OAuthToken.json pull -P <SOME_PARTICIPANT_GROUP> -C <SOME_COLUMN_GROUP>
```

Details on the use of pepcli can be found on the page [Using pepcli](using-pepcli.md)