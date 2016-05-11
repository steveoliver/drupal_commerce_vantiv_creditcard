<?php
/**
 * @file
 * Contains \Drupal\Tests\commerce_vantiv_creditcard\Kernel\CommerceVantivCreditCardTest;
 */

namespace Drupal\Tests\commerce_vantiv_creditcard\Kernel;

use Drupal\commerce_vantiv_creditcard\CommerceVantivCreditCard;
use Drupal\KernelTests\KernelTestBase;

/**
 * @coversDefaultClass \Drupal\commerce_vantiv_creditcard\CommerceVantivCreditCard
 *
 * @group commerce_vantiv_creditcard
 */
class CommerceVantivCreditCardTest extends KernelTestBase {

  /**
   * Modules to enable.
   *
   * @var array
   */
  public static $modules = ['system', 'user', 'commerce_vantiv_creditcard'];

  /**
   * {@inheritdoc}
   */
  protected function setUp() {
    parent::setUp();
    // Do something with the database.
    $this->installSchema('system', 'router');
  }

  /**
   * @covers ::bar
   */
  public function testKernelTestBase() {
    $test = new CommerceVantivCreditCard();
    $this->assertEquals('foo', $test->bar());
    $this->assertEquals('foo', 'bar');
  }
}

