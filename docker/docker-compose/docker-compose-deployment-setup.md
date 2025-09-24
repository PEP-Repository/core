# Docker Compose Deployment Setup Guide

## Install the trigger script on the server

Copy the `example-trigger-compose.sh` script to your server at `/trigger-compose.sh`:

```bash
sudo cp example-trigger-compose.sh /trigger-compose.sh
sudo chmod +x /trigger-compose.sh
sudo chown root:root /trigger-compose.sh
```

## Set up Constellation JSON Configuration

Configure your constellation JSON file with the deploy-compose section:

```json
[
  {
    "name": "nolai-snappet-test.pep.cs.ru.nl",
    "ssh": {
      "user": {
        "name": "update",
        "id_file": "$GITLAB_GROUP_CI_BUILDSERVERS_LINUX_UPDATE_KEY_FILE"
      }
    },
    "deploy-compose": {
      "user": {
        "name": "update",
        "id_file": "$GITLAB_GROUP_CI_BUILDSERVERS_LINUX_UPDATE_KEY_FILE"
      }
    }
  }
]
```
