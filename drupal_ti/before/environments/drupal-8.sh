#!/bin/bash
# @file
# Drupal-8 environment variables.

export DRUPAL_TI_SIMPLETEST_GROUP="commerce_vantiv_creditcard_group";

# Override the install profile for Drupal 8.
# This is needed as behat does not work with minimal right now.
export DRUPAL_TI_INSTALL_PROFILE="standard";

# Make a place for Behat Mink Debug Extension logs.
mkdir $TRAVIS_BUILD_DIR/$DRUPAL_TI_BEHAT_DIR/logs
