#!/bin/bash
# Example trigger script for server-side docker-compose deployment
# Place this script at /trigger-compose.sh on your deployment server
# Configure SSH authorized_keys with: command="sudo /trigger-compose.sh $SSH_ORIGINAL_COMMAND"

set -e

# Parse the action and image from the SSH command arguments
ACTION="$1"
IMAGE_COMPOSE="$2"

# Validate action
if [ "$ACTION" != "pull" ] && [ "$ACTION" != "restart" ] && [ "$ACTION" != "pull-restart" ]; then
    echo "Error: Unknown action: $ACTION (expected 'pull', 'restart', or 'pull-restart')"
    exit 1
fi

if [ -z "$IMAGE_COMPOSE" ]; then
    echo "Error: IMAGE_COMPOSE not provided"
    exit 1
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