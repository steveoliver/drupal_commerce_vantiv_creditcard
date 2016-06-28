#!/bin/bash

echo "WE ARE IN SIMPLETEST BEFORE_SCRIPT"

# Move to sites/all.
echo "TRAVIS_BUILD_DIR: $TRAVIS_BUILD_DIR"
cd "$TRAVIS_BUILD_DIR"
echo "ls -al"
ls -al
echo "cd ../"
cd ../
echo "ls -al"
ls -al
echo "ls -al drupal-7"
ls -al drupal-7
echo "Trying to get to <drupal root>/sites/all..."
echo "DRUPAL_TI_DRUPAL_DIR: $DRUPAL_TI_DRUPAL_DIR"
cd "$DRUPAL_TI_DRUPAL_DIR"
echo "DRUPAL_TI_MODULES_PATH: $DRUPAL_TI_MODULES_PATH"
cd "$DRUPAL_TI_MODULES_PATH"
echo "ls $DRUPAL_TI_MODULES_PATH:"
ls "$DRUPAL_TI_MODULES_PATH"
echo "cd ../"
cd ../
echo "ls:"
ls

# Make libraries directory.
mkdir -p libraries

# Download library dependencies.
git clone --depth 1 https://github.com/steveoliver/vantiv_devhub_sdk_php.git libraries/vantiv-devhub-php

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
