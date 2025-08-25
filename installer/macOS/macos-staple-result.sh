#!/usr/bin/env bash

set -exuo pipefail

echo "Stapling and checking notarization for the following products: $*"

for PRODUCT_PATH in "$@"; do
  xcrun stapler staple "$PRODUCT_PATH"
  spctl --assess --verbose=3 --type install "$PRODUCT_PATH"
done