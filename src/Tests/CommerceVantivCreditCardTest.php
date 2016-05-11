<?php
/**
 * @file
 * Contains \Drupal\commerce_vantiv_creditcard\Tests\CommerceVantivCreditCardTest.
 */

namespace Drupal\commerce_vantiv_creditcard\Tests;

use Drupal\simpletest\WebTestBase;
use Drupal\commerce_vantiv_creditcard\CommerceVantivCreditCard;

/**
 * Tests the CommerceVantivCreditCard implementation of the commerce_vantiv_creditcard module.
 *
 * @group commerce_vantiv_creditcard_group
 *
 * @codeCoverageIgnore
 */
class CommerceVantivCreditCardTest extends WebTestBase {

  /**
   * Modules to enable.
   *
   * @var array
   */
  public static $modules = array('commerce_vantiv_creditcard', 'node');

  /**
   * The basic functionality of the CommerceVantivCreditCard class.
   */
  public function testCommerceVantivCreditCard() {
    $test = new CommerceVantivCreditCard();
    $this->assertEqual('foo', $test->bar(), "Bar function of CommerceVantivCreditCard() returns foo.");
    $this->drupalGet('<front>');
    $this->assertResponse(200, 'Front page exists.');
    $this->assertRaw('Drupal');
    // Test that login works.
    $admin_user = $this->drupalCreateUser(array('access content'));
    $this->drupalLogin($admin_user);
  }
}
