#!/usr/bin/env bash

### Prerequisites: ###
# Install mermaid-cli if not available using the next line:
#> npm install -g @mermaid-js/mermaid-cli 
# Mermaid uses nodejs, to be installed via NVM (Node Version Manager), source https://nodejs.org/en/download/package-manager
#>  curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
# download and install Node.js
#> nvm install 20

mmdc -i hhair-sequences-data_store.mmd -e pdf
