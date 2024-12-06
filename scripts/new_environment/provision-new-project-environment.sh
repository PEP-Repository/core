#!/usr/bin/env bash

set -e
# This script will only work after having been pulled into release, as the submodules are set to release.

# Before running this script, make sure that:

# * All required steps in the section "Setting up a gitlab project" are completed, and instructions for 
#   how to use this script in the "Automated" section are followed
#   in https://docs.pages.pep.cs.ru.nl/private/ops/master/development/Create-new-project-and-test-environment/
# * The desired new project variables are set in new-project.conf

# The workflow of this script and the subsequent CI/CD operations are as follows:

# * The script will clone the new project repository and add the core and ops submodules.
# * Then it will copy the template configuration files from the environment_templates folder in core 
#   and replace the placeholders with the new project variables.
# * In the new project repository, the .template-gitlab-ci.yml will run and will generate the keys in a 
#   docker container which was generated in the pipeline, then change the .gitlab-ci.yml to new-project-verifiers.yml.
# * Next some manual steps have to be completed, then the job add-verifiers-to-new-project-COPY-KEYS-TO-VM-FIRST can be triggered.
#   This will run a job with a docker container which adds the verifiers. Finally, the .gitlab-ci.yml 
#   will be changed to the final CI.

TEMPLATE_CONFIG_BRANCH="release"

PEP_FOSS_REPO_DIR=$(readlink -f "$(dirname "$0")/../..")
PEP_DTAP_REPO_DIR=$(readlink -f "$(dirname "$0")/../../../ops")

find_rel_path() {
    s=$(cd ${1%%/};pwd)
    d=$(cd $2;pwd)
    b=;
  while [ "${d#$s/}" == "${d}" ]; do
    s=$(dirname $s)
    b="../${b}"
  done
  echo ${b}${d#$s/}
}

# Get new project config
# shellcheck disable=SC1091
source new-project.conf

NEW_PROJECT_REPO_DIR="$NEW_PROJECT_GROUP/$NEW_PROJECT_SUBGROUP_PATH/$NEW_PROJECT_SHORT_SLUG"
NEW_PROJECT_FULL_REPO_DIR="gitlabregistry.$NEW_PROJECT_DOMAIN_NAME/$NEW_PROJECT_REPO_DIR"
NEW_PROJECT_SERVER_DOMAIN_NAME="$NEW_PROJECT_FULL_SLUG.$NEW_PROJECT_DOMAIN_NAME"
NEW_PROJECT_LOGO_NAME=$(basename "$NEW_PROJECT_LOGO_PATH")

# Obtain gitlab password from ops
NEW_PROJECT_GITLAB_PASSWORD="$(cat "$PEP_DTAP_REPO_DIR/keys/gitlab-registry-token.txt")"

NEW_PROJECT_GITLAB_ACCESS_B64="$(echo -n "${NEW_PROJECT_GITLAB_USER_NAME}:${NEW_PROJECT_GITLAB_PASSWORD}" | base64)"

# check existence PEP_FOSS_REPO_DIR
if ! [ -d "$PEP_FOSS_REPO_DIR" ]; then
    echo "Variable PEP_FOSS_REPO_DIR not set or not a directory"
    exit 1
fi

cd "$PEP_FOSS_REPO_DIR"

current_branch="$(git rev-parse --revs-only --verify --abbrev-ref HEAD)"
if [ "$current_branch" != "$TEMPLATE_CONFIG_BRANCH" ]; then
  echo "You are executing the provisioning script from branch '$current_branch', are you sure you don't want to provision the environment based on '$TEMPLATE_CONFIG_BRANCH'?"
  echo "Continue? [y/n]?"
  read -r choice
  case "$choice" in
    [Yy]*) ;;
    *)
      >&2 echo Aborting
      exit 1 ;;
  esac
fi

cd ../

# check if NEW_PROJECT_SUBGROUP_PATH variable exists
if [ -z "$NEW_PROJECT_SUBGROUP_PATH" ]; then
    echo "Variable NEW_PROJECT_SUBGROUP_PATH is not set"
    exit 1
else
    # check if directory $NEW_PROJECT_SUBGROUP_PATH exists
    if [ -d "$NEW_PROJECT_SUBGROUP_PATH" ]; then
        echo "Directory $NEW_PROJECT_SUBGROUP_PATH already exists"
        cd "$NEW_PROJECT_SUBGROUP_PATH"
    else
        mkdir -p "$NEW_PROJECT_SUBGROUP_PATH"
        cd "$NEW_PROJECT_SUBGROUP_PATH"
    fi
fi

# Clone the project repository
git clone "https://gitlab.$NEW_PROJECT_DOMAIN_NAME/$NEW_PROJECT_REPO_DIR.git"
if ! cd "$NEW_PROJECT_SHORT_SLUG"; then
  echo "Failed to correctly clone $NEW_PROJECT_SHORT_SLUG"
  exit 1
fi

git checkout "$NEW_PROJECT_BRANCH"
git submodule add -b "$TEMPLATE_CONFIG_BRANCH" "$(find_rel_path "$(pwd)" "$PEP_FOSS_REPO_DIR").git" pep
git submodule add -b "$TEMPLATE_CONFIG_BRANCH" "$(find_rel_path "$(pwd)" "$PEP_DTAP_REPO_DIR").git" dtap
git submodule update --init --recursive

ssh-keygen -f "pep-$NEW_PROJECT_FULL_SLUG" -C "pep-$NEW_PROJECT_FULL_SLUG" -N '' -t ed25519
NEW_PROJECT_SSH_PRIVATE_KEY="$(jq --raw-input --slurp . "pep-$NEW_PROJECT_FULL_SLUG")"
NEW_PROJECT_SSH_PUBLIC_KEY="$(jq --raw-input --slurp . "pep-$NEW_PROJECT_FULL_SLUG.pub")"
rm "pep-$NEW_PROJECT_FULL_SLUG" "pep-$NEW_PROJECT_FULL_SLUG.pub"

# Copy template config
mkdir -p "config/$NEW_PROJECT_BRANCH"
cp -r "$PEP_FOSS_REPO_DIR/environment_templates/config/"* "config/$NEW_PROJECT_BRANCH"

# Copy the logo to the directory
cp "$NEW_PROJECT_LOGO_PATH" "config/$NEW_PROJECT_BRANCH/project/client/$NEW_PROJECT_LOGO_NAME"

# CP VM folder
cp -r "$PEP_FOSS_REPO_DIR/environment_templates/vm" .

# CP TEMPLATE CMAKELISTS
cp "$PEP_FOSS_REPO_DIR/environment_templates/template-CMakeLists.txt" "CMakeLists.txt"

# CP .gitlab-ci.yml
cp "$PEP_FOSS_REPO_DIR/environment_templates/.template-gitlab-ci.yml" ".gitlab-ci.yml"
cp "$PEP_FOSS_REPO_DIR/environment_templates/new-project-keys.yml" .
cp "$PEP_FOSS_REPO_DIR/environment_templates/new-project-verifiers.yml" .

# Define your placeholders and replacements
NEW_PROJECT_PLACEHOLDERS=(
    "@@SSH_PRIVATE_KEY_PLACEHOLDER@@"
    "@@SSH_PUBLIC_KEY_PLACEHOLDER@@"
    "@@SERVER_PLACEHOLDER@@"
    "@@LOGO_PLACEHOLDER@@"
    "@@REPO_DIR_PLACEHOLDER@@"
    "@@ACRONYM_PLACEHOLDER@@"
    "@@PROJECT_NAME_PLACEHOLDER@@"
    "@@PROJECT_FULL_SLUG_PLACEHOLDER@@"
    "@@FULL_REPO_DIR_PLACEHOLDER@@"
    "@@DOMAIN_NAME_PLACEHOLDER@@"
    "@@SERVER_LOCATION_PLACEHOLDER@@"
    "@@GITLAB_ACCESS_B64_PLACEHOLDER@@"
    "@@BRANCH_PLACEHOLDER@@"
    "@@PROJECT_MEDIUM_SLUG_PLACEHOLDER@@"
)

NEW_PROJECT_REPLACEMENTS=(
    "$NEW_PROJECT_SSH_PRIVATE_KEY"
    "$NEW_PROJECT_SSH_PUBLIC_KEY"
    "$NEW_PROJECT_SERVER_DOMAIN_NAME"
    "$NEW_PROJECT_LOGO_NAME"
    "$NEW_PROJECT_REPO_DIR"
    "$NEW_PROJECT_ACRONYM"
    "$NEW_PROJECT_NAME"
    "$NEW_PROJECT_FULL_SLUG"
    "$NEW_PROJECT_FULL_REPO_DIR"
    "$NEW_PROJECT_DOMAIN_NAME"
    "$NEW_PROJECT_SERVER_LOCATION"
    "$NEW_PROJECT_GITLAB_ACCESS_B64"
    "$NEW_PROJECT_BRANCH"
    "$NEW_PROJECT_MEDIUM_SLUG"
)

# Define your list of files
NEW_PROJECT_FILES=(
    "config/$NEW_PROJECT_BRANCH/authserver_apache/etc/apache2/conf-available/000-servername.conf"
    "config/$NEW_PROJECT_BRANCH/client/ClientConfig.json"
    "config/$NEW_PROJECT_BRANCH/nginx/etc/nginx.conf"
    "config/$NEW_PROJECT_BRANCH/pep-monitoring/watchdog/config.yaml"
    "config/$NEW_PROJECT_BRANCH/pep-monitoring/watchdog/constellation.yaml"
    "config/$NEW_PROJECT_BRANCH/pep-services/accessmanager/AccessManager.json"
    "config/$NEW_PROJECT_BRANCH/pep-services/authserver/Authserver.json"
    "config/$NEW_PROJECT_BRANCH/pep-services/registrationserver/RegistrationServer.json"
    "config/$NEW_PROJECT_BRANCH/pep-services/storagefacility/StorageFacility.json"
    "config/$NEW_PROJECT_BRANCH/pep-services/transcryptor/Transcryptor.json"
    "config/$NEW_PROJECT_BRANCH/project/accessmanager/GlobalConfiguration.json"
    "config/$NEW_PROJECT_BRANCH/project/client/ProjectConfig.json"
    "config/$NEW_PROJECT_BRANCH/project/caption.txt"
    "config/$NEW_PROJECT_BRANCH/constellation.json"
    "config/$NEW_PROJECT_BRANCH/rootCA.cert"
    "config/$NEW_PROJECT_BRANCH/ShadowAdministration.pub"
    "vm/pep-vm-template.yaml"
    "CMakeLists.txt"
    ".gitlab-ci.yml"
    "new-project-keys.yml"
    "new-project-verifiers.yml"
)

# Loop over each file
for NEW_PROJECT_FILE in "${NEW_PROJECT_FILES[@]}"; do
  # Loop over each placeholder/replacement pair
  for PLACEHOLDER_INDEX in "${!NEW_PROJECT_PLACEHOLDERS[@]}"; do
    # Use sed to replace the placeholder with the replacement
    ESCAPED_REPLACE=$(printf '%s' "${NEW_PROJECT_REPLACEMENTS[$PLACEHOLDER_INDEX]}" | sed -e 's/[\/&]/\\&/g')
    sed "s|${NEW_PROJECT_PLACEHOLDERS[$PLACEHOLDER_INDEX]}|$ESCAPED_REPLACE|g" >"$NEW_PROJECT_FILE.tmp"
    mv "$NEW_PROJECT_FILE.tmp" "$NEW_PROJECT_FILE"
  done
done

# Rename the VM template
mv "vm/pep-vm-template.yaml" "vm/$NEW_PROJECT_SERVER_LOCATION-pep-$NEW_PROJECT_FULL_SLUG.yaml"

# TODO: SOMETHING TO GENERATE rest of GlobalConfiguration project structure for this project

# GUID windowsinstaller
NEW_PROJECT_UUID=$(uuidgen)

# Write the XML content to the file
cat > "config/$NEW_PROJECT_BRANCH/WindowsInstaller.wxi" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<Include xmlns="http://schemas.microsoft.com/wix/2006/wi">

<?define UpgradeCode = "{$NEW_PROJECT_UUID}" ?>

</Include>
EOF

echo "---------------------------------------"
echo "Put the following in ops/sshconfig file in the release branch and merge to stable and then master (similarly update the known_hosts after having connected):"
cat << EOF
Host "pep-$NEW_PROJECT_FULL_SLUG"
  Hostname "$NEW_PROJECT_FULL_SLUG.$NEW_PROJECT_DOMAIN_NAME"
  User pep 
  IdentityFile ~/.ssh/pep/buildservers 
  ProxyJump lilo
EOF
echo "---------------------------------------"

# Check if TEMPLATE_CONFIG_BRANCH is not release
if [ "$TEMPLATE_CONFIG_BRANCH" != "release" ]; then
  echo "Setting pep and dtap submodules back to release branch"
  cd pep && git checkout release && git pull origin release && cd -
  cd dtap && git checkout release && git pull origin release && cd -
fi

# Stage, commit and push the changes
git add . 
git commit -m "Initialized new project"
git push