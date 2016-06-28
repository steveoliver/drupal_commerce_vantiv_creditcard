#!/bin/bash
# @file
# Simple script to run the tests.

set -e

# Goto current directory.
DIR=$(dirname $0)
cd $DIR

# Download required dependencies.
drush make project.make.yml

# Enable hard dependencies
drush en -y rules
