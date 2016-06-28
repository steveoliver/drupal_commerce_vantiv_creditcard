# Drupal Commerce Vantiv Credit Card

[![Build Status](https://travis-ci.org/steveoliver/drupal_commerce_vantiv_creditcard.svg?branch=7.x-1.x)](https://travis-ci.org/steveoliver/drupal_commerce_vantiv_creditcard)

# Drupal Commerce 1.x (Drupal 7) payment method

Integrates [Vantiv online payments](https://www.vantiv.com/online-payments) with Drupal Commerce (7.x-1.x). Implements [eProtect](https://www.vantiv.com/developers/ecommerce-payments#security-features) to reduce PCI DSS v3.1 controls. Supports [Card on file](https://www.drupal.org/project/commerce_cardonfile). Supports terminal (Drupal Commerce back-end) payments.

# Dependencies
 - [Drupal Commerce](https://www.drupal.org/project/commerce)
 - [Libraries](https://www.drupal.org/project/libraries)
 - [Vantiv PHP SDK](https://github.com/steveoliver/vantiv_devhub_sdk_php)
 - [Commerce Card on File](https://www.drupal.org/project/commerce_cardonfile) (optional)

# Prerequisites
 - A Vantiv DevHub account and application. [Sign up](https://apideveloper.vantiv.com/).

# Installation
 - Download Drupal Commerce and Libraries modules in [sites/all] `modules` directory.
 - Download Commerce Card on Files module to [sites/all] `modules` (optional).
 - Download Vantiv PHP SDK to [sites/all/libraries] `vantiv-php-sdk`.
 - Enable Drupal Commerce + dependencies and Commerce Vantiv Credit Card module.
 - Enable `Credit Card via Vantiv` payment method.
 - Configure payment method with License Key and other options.

