#!/bin/bash

set -e $DRUPAL_TI_DEBUG

echo "BEFORE SCRIPT!"
echo "DRUPAL DIR: $DRUPAL_TI_DRUPAL_DIR"

# Go to Drupal root directory.
cd "$DRUPAL_TI_DRUPAL_DIR/$DRUPAL_TI_MODULES_PATH"

# Enable hard dependencies
drush en -y rules
