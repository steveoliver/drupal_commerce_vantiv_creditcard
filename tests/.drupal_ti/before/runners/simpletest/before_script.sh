#!/bin/bash

set -e $DRUPAL_TI_DEBUG

# Ensure the right Drupal version is installed.
drupal_ti_ensure_drupal

# Move to sites/all
cd "$DRUPAL_TI_DRUPAL_DIR/$DRUPAL_TI_MODULES_PATH/../"

# Make libraries directory.
mkdir -p libraries

# Download library dependencies.
git clone --depth 1 https://github.com/steveoliver/vantiv_devhub_sdk_php.git libraries/vantiv-php-sdk

# Download module dependencies.
mkdir -p modules
cd modules
drush dl --yes \
  addressfield-7.x-1.2 \
  ctools-7.x-1.9 \
  entity-7.x-1.7 \
  libraries-7.x-2.2 \
  rules-7.x-2.9 \
  views-7.x-3.14 \
  commerce-7.x-1.13 \
  jquery_update-7.x-3.x \
  commerce_cardonfile-7.x-2.x \
  commerce_recurring-7.x-2.x \
  interval-7.x-1.0

# Enable hard dependencies
drush en -y rules
