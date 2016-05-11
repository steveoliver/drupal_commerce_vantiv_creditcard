<?php
/**
 * @file
 * Contains \Drupal\commerce_vantiv_creditcard\Unit\CommerceVantivCreditCardTest.
 */

namespace \Drupal\Tests\commerce_vantiv_creditcard\Unit;

use Drupal\commerce_vantiv_creditcard\CommerceVantivCreditCard;

/**
 * @coversDefaultClass \Drupal\commerce_vantiv_creditcard\CommerceVantivCreditCard
 *
 * @group commerce_vantiv_creditcard
 */
class CommerceVantivCreditCardTest extends \PHPUnit_Framework_TestCase {

  /**
   * @covers ::bar
   */
  public function test_bar() {
    $test = new CommerceVantivCreditCard();
    $this->assertEquals('foo', $test->bar());
  }
}

