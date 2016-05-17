#!/bin/bash
# @file
# Performs actions after a Drupal-8 environment build.

# Upload Behat Mink Debug Extension logs.
cd $TRAVIS_BUILD_DIR
cd $DRUPAL_TI_BEHAT_DIR

./vendor/bin/upload-textfiles "logs/*.log"
