<?php

/**
 * @file
 * Administrative forms for the Commerce Vantiv Credit Card module.
 */

/**
 * Form callback: allows the user to capture a prior authorization.
 */
function commerce_vantiv_creditcard_capture_form($form, &$form_state, $order, $transaction) {
  $form_state['order'] = $order;
  $form_state['transaction'] = $transaction;

  // Load and store the payment method instance for this transaction.
  $payment_method = commerce_payment_method_instance_load($transaction->instance_id);
  $form_state['payment_method'] = $payment_method;

  $balance = commerce_payment_order_balance($order);

  if ($balance['amount'] > 0 && $balance['amount'] < $transaction->amount) {
    $default_amount = $balance['amount'];
  }
  else {
    $default_amount = $transaction->amount;
  }

  // Convert the price amount to a user friendly decimal value.
  $default_amount = commerce_vantiv_creditcard_amount_to_decimal($default_amount, $transaction->currency_code);

  $description = implode('<br />', array(
    t('Authorization: @amount', array('@amount' => commerce_currency_format($transaction->amount, $transaction->currency_code))),
    t('Order balance: @balance', array('@balance' => commerce_currency_format($balance['amount'], $balance['currency_code']))),
  ));

  $form['amount'] = array(
    '#type' => 'textfield',
    '#title' => t('Capture amount'),
    '#description' => $description,
    '#default_value' => $default_amount,
    '#field_suffix' => check_plain($transaction->currency_code),
    '#size' => 16,
  );

  $form = confirm_form($form,
    t('What amount do you want to capture?'),
    'admin/commerce/orders/' . $order->order_id . '/payment',
    '',
    t('Capture'),
    t('Cancel'),
    'confirm'
  );

  return $form;
}

/**
 * Validate handler: ensure a valid amount is given.
 */
function commerce_vantiv_creditcard_capture_form_validate($form, &$form_state) {
  $transaction = $form_state['transaction'];
  $amount = $form_state['values']['amount'];
  $card_type = $transaction->data['card_type'];

  // Ensure a positive numeric amount has been entered for capture.
  if (!is_numeric($amount) || $amount <= 0) {
    form_set_error('amount', t('You must specify a positive numeric amount to capture.'));
  }

  // Ensure the amount is less than or equal to the authorization amount.
  if ($amount > commerce_currency_amount_to_decimal($transaction->amount, $transaction->currency_code)) {
    form_set_error('amount', t('You cannot capture more than you authorized.'));
  }

  // If the authorization has expired, display an error message and redirect.
  if (time() - $transaction->created > _commerce_vantiv_creditcard_authorization_lifespan($card_type)) {
    drupal_set_message(t('This authorization has passed its limit and cannot be captured.'), 'error');
    drupal_goto('admin/commerce/orders/' . $form_state['order']->order_id . '/payment');
  }
}

/**
 * Submit handler: process a prior authorization capture via AIM.
 */
function commerce_vantiv_creditcard_capture_form_submit($form, &$form_state) {
  $transaction = $form_state['transaction'];
  $amount = number_format($form_state['values']['amount'], 2, '.', '');
  $payment_method = $form_state['payment_method'];
  $order = $form_state['order'];
  if (!commerce_vantiv_creditcard_load_library()) {
    return FALSE;
  }

  // Initialize configuration.
  $config = new Vantiv\Configuration([
    'license' => _commerce_vantiv_creditcard_get_license($payment_method),
    'api_version' => _commerce_vantiv_creditcard_get_api_version($payment_method),
    'base_url' => _commerce_vantiv_creditcard_get_base_url($payment_method)
  ]);
  // Initialize request.
  $request = new Vantiv\Request\Credit\AuthorizationCompletion($config);
  $request_body = [
    'Credentials' => [
      'AcceptorID' => _commerce_vantiv_creditcard_get_merchant_id($payment_method)
    ],
    'Reports' => [
      'ReportGroup' => _commerce_vantiv_creditcard_get_report_group($payment_method, $order)
    ],
    'Transaction' => [
      'TransactionID' => $transaction->remote_id,
      'TransactionAmount' => $amount,
    ],
    'Application' => [
      'ApplicationID' => $order->order_id . 'capture'
    ],
  ];
  if ($amount < commerce_currency_amount_to_decimal($transaction->amount, $transaction->currency_code)) {
    $request_body['Transaction']['PartialCapture'] = 'true'; // Vantiv requires BOOL as string.
  }
  // Perform request and prepare response.
  $response = $request->send($request_body);
  $response_body = $response['response']->getResponse();
  $request_id = $response['response']->getRequestID();
  $response_code = $response['http_code'];

  // Update and save the transaction based on the response.
  $log = [];
  if ($payment_method['settings']['log']['request']) {
    $log['request'] = [
      'type' => get_class($request),
      'body' => $request_body
    ];
  }
  else {
    $log['request'] = t('Logging not enabled for requests. Enable at @url', [
      '@url' => 'admin/commerce/config/payment-methods'
    ]);
  }
  if ($payment_method['settings']['log']['response']) {
    $log['response'] = [
      'type' => get_class($response['response']),
      'body' => $response_body
    ];
  }
  else {
    $log['response'] = t('Logging not enabled for responses. Enable at @url', [
      '@url' => 'admin/commerce/config/payment-methods'
    ]);
  }
  $transaction->payload[$request_id] = $log;

  // If we didn't get an approval response code...
  if ($response_code !== 200 || !in_array($response_body->response, [000, 001, 010])) {
    // Display an error message but leave the transaction pending.
    drupal_set_message(t('Prior authorization capture failed, so the transaction will remain in a pending status.'), 'error');
    drupal_set_message(check_plain($response_body['message']), 'error');
  }
  else {
    drupal_set_message(t('Prior authorization captured successfully.'));

    // Update the transaction amount to the actual capture amount.
    $transaction->amount = commerce_currency_decimal_to_amount($amount, $transaction->currency_code);

    // Set the remote and local status accordingly.
    $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
    $transaction->remote_status = COMMERCE_CREDIT_AUTH_CAPTURE;

    // Set the remote id to the capture response transaction id.
    $transaction->remote_id = $response_body->TransactionID;

    // Add status and capture date to the result message.
    $transaction->message = implode('<br />', [
        $transaction->status,
        t('Captured: @date', array('@date' => format_date(REQUEST_TIME, 'short')))
    ]);
  }

  commerce_payment_transaction_save($transaction);

  // @todo: Reverse unused authorization amount for cards other than VISA or AMEX.
  // @see 'reversal Transcations' in DevHub_Integration_Guidelines PDF.

  $form_state['redirect'] = 'admin/commerce/orders/' . $form_state['order']->order_id . '/payment';
}
