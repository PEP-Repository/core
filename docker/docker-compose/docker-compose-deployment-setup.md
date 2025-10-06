# Docker Compose Deployment Setup Guide

## Install the trigger script on the server

make sure a `/trigger-compose.sh` script (example provided in this document) is on the server.

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
    "docker-compose": {
      "user": {
        "name": "update",
        "id_file": "$GITLAB_GROUP_CI_BUILDSERVERS_LINUX_COMPOSE_KEY_FILE"
      },
      "profiles": ["accessmanager", "registrationserver", "authserver", "nginx"]
    }
  }
]
```

## Available Commands

### redeploy (default)

- Pulls latest docker-compose image
- Stops and removes existing containers  
- Starts new containers with fresh images
- Can start specific services with comma separated list of profiles

### pull

- Only pulls the latest docker-compose image
- Does not affect running services

### restart

- Restarts services without pulling new images
- Can restart specific service with comma separated list of profiles

### stop

- Stops services without removing containers
- Can stop specific service with comma separated list of profiles e.g.

```bash
./constellation.sh config.json deploy-compose "$IMAGE" stop transcryptor,keyserver
```

## Example compose script server-side

```bash
#!/usr/bin/env bash
# Example trigger script for server-side docker-compose deployment
# Place this script at /trigger-compose.sh on the server where the services should be deployed
# Configure SSH authorized_keys with: command="sudo /trigger-compose.sh $SSH_ORIGINAL_COMMAND"

set -e

ACTION="$1"
IMAGE_COMPOSE="$2"
COMPOSE_PROFILES="${3:-}"  # Optional third argument for profiles, e.g. "accessmanager,nginx"

# Validate action
if [ "$ACTION" != "pull" ] && [ "$ACTION" != "restart" ] && [ "$ACTION" != "stop" ] && [ "$ACTION" != "redeploy" ]; then
    echo "Error: Unknown action: $ACTION (expected 'pull', 'restart', 'stop', or 'redeploy')"
    exit 1
fi

if [ -z "$IMAGE_COMPOSE" ]; then
    echo "Error: IMAGE_COMPOSE not provided"
    exit 1
fi

echo "Docker-compose action triggered: $ACTION"
echo "Using image: $IMAGE_COMPOSE"

# Use profiles from argument if provided, otherwise let docker-compose use .env defaults
if [ -n "$COMPOSE_PROFILES" ]; then
    echo "Using compose profiles: $COMPOSE_PROFILES"
    PROFILES_ENV_FLAG="--env COMPOSE_PROFILES=$COMPOSE_PROFILES"
else
    echo "Using compose profiles from .env file"
    PROFILES_ENV_FLAG=""
fi

# Common function to run docker compose commands
run_compose_command() {
    local compose_command="$1"

    /usr/bin/docker stop docker-compose || true

    /usr/bin/docker run --privileged \
                       --name "docker-compose" \
                       --rm \
                       --volume /root/.docker/config.json:/root/.docker/config.json:ro \
                       --volume /var/run/docker.sock:/var/run/docker.sock \
                       $PROFILES_ENV_FLAG \
                       "$IMAGE_COMPOSE" \
                       docker compose --file compose.yml \
                                      --file compose.override.yaml \
                                      --env-file .env \
                                      $compose_command
}

case "$ACTION" in
    "pull")
        echo "Pulling latest docker-compose image..."
        /usr/bin/docker pull "$IMAGE_COMPOSE"
        echo "Pull completed successfully"
        ;;
    "restart")
        echo "Restarting services..."
        run_compose_command "restart"
        echo "Services restarted successfully"
        ;;
    "stop")
        echo "Stopping services..."
        run_compose_command "stop"
        echo "Services stopped successfully"
        ;;
    "redeploy")
        echo "Redeploying services..."
        
        echo "Pulling latest docker-compose image..."
        /usr/bin/docker pull "$IMAGE_COMPOSE"
        
        echo "Force-recreating services with new images..."
        run_compose_command "up --detach --pull always --force-recreate"

        echo "Redeploy completed successfully"
        ;;
    *)
        echo "Error: Unknown action: $ACTION"
        exit 1
        ;;
esac
```
