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
        "id_file": "$GITLAB_GROUP_CI_BUILDSERVERS_LINUX_COMPOSE_KEY_FILE"
      },
      "profiles": ["accessmanager", "registrationserver", "authserver", "nginx"]
    }
  }
]
```

## Example compose script server-side

```bash
#!/usr/bin/env bash
# Example trigger script for server-side docker-compose deployment
# Place this script at /trigger-compose.sh on your deployment server
# Configure SSH authorized_keys with: command="sudo /trigger-compose.sh $SSH_ORIGINAL_COMMAND"

set -e

# Parse the action and image from the SSH command arguments
ACTION="$1"
IMAGE_COMPOSE="$2"
COMPOSE_PROFILES="${3:-}"  # Optional comma-separated profiles, e.g. "transcryptor,keyserver"

# Validate action
if [ "$ACTION" != "pull" ] && [ "$ACTION" != "restart" ] && [ "$ACTION" != "pull-restart" ]; then
    echo "Error: Unknown action: $ACTION (expected 'pull', 'restart', or 'pull-restart')"
    exit 1
fi

if [ -z "$IMAGE_COMPOSE" ]; then
    echo "Error: IMAGE_COMPOSE not provided"
    exit 1
fi

# Use profiles from argument if provided, otherwise let docker-compose use the profiles from .env
if [ -n "$COMPOSE_PROFILES" ]; then
    echo "Overriding compose profiles: $COMPOSE_PROFILES"
    PROFILES_ENV_FLAG="--env COMPOSE_PROFILES=$COMPOSE_PROFILES"
else
    echo "Using compose profiles from .env file"
    PROFILES_ENV_FLAG=""
fi

echo "Docker-compose action triggered: $ACTION"
echo "Using image: $IMAGE_COMPOSE"

case "$ACTION" in
    "pull")
        echo "Pulling latest docker-compose image..."
        /usr/bin/docker pull "$IMAGE_COMPOSE"
        echo "Pull completed successfully"
        ;;
    "restart")
        echo "Restarting docker-compose services..."
        /usr/bin/docker run --privileged \
                           --name compose-restart \
                           --rm \
                           --volume /root/.docker/config.json:/root/.docker/config.json:ro \
                           --volume /var/run/docker.sock:/var/run/docker.sock \
                           $PROFILES_ENV_FLAG \
                           "$IMAGE_COMPOSE" \
                           docker compose --file compose.yml \
                                          --file compose.override.yaml \
                                          --env-file .env \
                                          up --force-recreate --detach --no-pull
        echo "Restart completed successfully"
        ;;
    "pull-restart")
        echo "Pulling latest docker-compose image..."
        /usr/bin/docker pull "$IMAGE_COMPOSE"
        
        echo "Restarting docker-compose services with new image..."
        /usr/bin/docker run --privileged \
                           --name compose-deploy \
                           --rm \
                           --volume /root/.docker/config.json:/root/.docker/config.json:ro \
                           --volume /var/run/docker.sock:/var/run/docker.sock \
                            $PROFILES_ENV_FLAG \
                           "$IMAGE_COMPOSE" \
                           docker compose --file compose.yml \
                                          --file compose.override.yaml \
                                          --env-file .env \
                                          up --force-recreate --detach --pull always
        echo "Pull and restart completed successfully"
        ;;
    *)
        echo "Error: Unknown action: $ACTION"
        exit 1
        ;;
esac
```
