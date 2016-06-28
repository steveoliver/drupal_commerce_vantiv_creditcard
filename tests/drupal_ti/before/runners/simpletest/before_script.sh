#!/bin/bash

set -e

# Go to Drupal root directory.
cd "$DRUPAL_TI_DRUPAL_DIR/$DRUPAL_TI_MODULES_PATH"

# Enable hard dependencies
drush en -y rules
