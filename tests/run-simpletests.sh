#!/bin/bash
# @file
# Simple script to run the tests.

set -e

# Goto current directory.
DIR=$(dirname $0)
cd $DIR

drush -y en simpletest commerce_vantiv_creditcard
drush test-run "CommerceVantivCreditCardTest" "$@"
