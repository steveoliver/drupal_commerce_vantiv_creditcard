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
  $default_amount = $transaction->data['auth_amount'];

  // Convert the price amount to a user friendly decimal value.
  $default_amount = commerce_vantiv_creditcard_amount_to_decimal($default_amount, $transaction->currency_code);

  $description = implode('<br />', array(
    t('Remaining authorization: @amount', [
      '@amount' => commerce_currency_format($transaction->data['auth_amount'], $transaction->currency_code)
    ]),
    t('Order balance: @balance', [
      '@balance' => commerce_currency_format($balance['amount'], $balance['currency_code'])
    ]),
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
  $card_type = !empty($transaction->data['card_type']) ? $transaction->data['card_type'] : FALSE;

  // Ensure a positive numeric amount has been entered for capture.
  if (!is_numeric($amount) || $amount <= 0) {
    form_set_error('amount', t('You must specify a positive numeric amount to capture.'));
  }

  // Ensure the amount is less than or equal to the authorization amount.
  if ($amount > commerce_currency_amount_to_decimal($transaction->data['auth_amount'], $transaction->currency_code)) {
    form_set_error('amount', t('You cannot capture more than you authorized.'));
  }

  // If the authorization has expired, display an error message and redirect.
  if ($card_type && time() - $transaction->created > _commerce_vantiv_creditcard_authorization_lifespan($card_type)) {
    drupal_set_message(t('This authorization has passed its limit and cannot be captured.'), 'error');
    drupal_goto('admin/commerce/orders/' . $form_state['order']->order_id . '/payment');
  }
}

/**
 * Submit handler: process a prior authorization capture via Vantiv.
 */
function commerce_vantiv_creditcard_capture_form_submit($form, &$form_state) {
  if (!commerce_vantiv_creditcard_load_library()) {
    return FALSE;
  }

  $payment_method = $form_state['payment_method'];
  $transaction = $form_state['transaction'];
  $order = $form_state['order'];

  // Format amount for different uses.
  $decimal_amount = number_format($form_state['values']['amount'], 2, '.', '');
  $amount = commerce_currency_decimal_to_amount($decimal_amount, $transaction->currency_code);
  $currency_amount = commerce_currency_format($amount, $transaction->currency_code);

  // Initialize request.
  $config = _commerce_vantiv_creditcard_get_config($payment_method);
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
      'TransactionAmount' => $decimal_amount,
      // Vantiv requires PartialCapture boolean as a string.
      'PartialCapture' => ($decimal_amount < $transaction->data['auth_amount']) ? 'true' : 'false',
    ],
    'Application' => [
      'ApplicationID' => $order->order_id . 'capture'
    ],
  ];

  // Create a new transaction to record this capture.
  $capture_transaction = commerce_payment_transaction_new($payment_method['method_id'], $order->order_id);
  $capture_transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;

  // Copy properties from authorization transaction.
  $capture_transaction->instance_id = $transaction->instance_id;
  $capture_transaction->currency_code = $transaction->currency_code;

  // Save capture transaction so ID can be used in request_body.
  commerce_payment_transaction_save($capture_transaction);

  // Append new transaction id to API request to ensure uniqueness.
  $request_body['Application']['ApplicationID'] .= $capture_transaction->transaction_id;

  // Perform API request and prepare response.
  $response = $request->send($request_body);
  $response_body = $response['response']->getResponse();
  $request_id = $response['response']->getRequestID();
  $response_code = $response['http_code'];

  // Add log entry to transaction.
  $capture_transaction->payload[$request_id] = _commerce_vantiv_creditcard_transaction_log($payment_method, $request, $request_body, $response, $response_body);

  // Set error messaging on failure or update auth transaction on success.
  if ($response_code !== 200 || !in_array($response_body->response, [000, 001, 010])) {
    drupal_set_message(t('Prior authorization capture failed, so the transaction will remain in a pending status.'), 'error');
    drupal_set_message(check_plain($response_body['message']), 'error');
  }
  else {
    drupal_set_message(t('Prior authorization captured successfully.'));

    // Set transaction amount to amount of this capture.
    $capture_transaction->amount = $amount;

    // Update original auth amount to reflect this capture.
    // A non-zero value allows additional captures/reversals.
    $transaction->data['auth_amount'] -= $amount;

    // Set remote id with capture transaction id.
    $capture_transaction->remote_id = $response_body->TransactionID;

    // Set local and remote status.
    $capture_transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
    $capture_transaction->remote_status = COMMERCE_CREDIT_AUTH_CAPTURE;

    // Add capture transaction event to authorization transaction message.
    $transaction->message .= '<br />' . t('(!txn) Capture @amount (@date) (Bal: @bal)', [
      '!txn' => l($capture_transaction->transaction_id, 'admin/commerce/orders/' . $order->order_id . '/payment/' . $capture_transaction->transaction_id . '/view'),
      '@amount' => $currency_amount,
      '@date' => format_date(REQUEST_TIME, 'short'),
      '@bal' => commerce_currency_format($transaction->data['auth_amount'], $transaction->currency_code)
    ]);

    // Log capture message on capture transaction.
    $capture_transaction->message = t('(!txn) Capture @amount (@date)', [
      '!txn' => l($transaction->transaction_id, 'admin/commerce/orders/' . $order->order_id . '/payment/' . $transaction->transaction_id . '/view'),
      '@amount' => $currency_amount,
      '@date' => format_date(REQUEST_TIME, 'short')
    ]);

    // Set auth transaction status as success once depleted.
    if (empty($transaction->data['auth_amount'])) {
      $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
    }

    commerce_payment_transaction_save($transaction);
  }

  commerce_payment_transaction_save($capture_transaction);

  $form_state['redirect'] = 'admin/commerce/orders/' . $form_state['order']->order_id . '/payment';
}

/**
 * Form callback: allows the user to reverse a prior authorization.
 */
function commerce_vantiv_creditcard_reversal_form($form, &$form_state, $order, $transaction) {
  $form_state['order'] = $order;
  $form_state['transaction'] = $transaction;

  // Load and store the payment method instance for this transaction.
  $payment_method = commerce_payment_method_instance_load($transaction->instance_id);
  $form_state['payment_method'] = $payment_method;

  $balance = commerce_payment_order_balance($order);

  // Convert the price amount to a user friendly decimal value.
  $default_amount = commerce_vantiv_creditcard_amount_to_decimal($transaction->data['auth_amount'], $transaction->currency_code);

  $description = implode('<br />', array(
    t('Remaining authorization: @amount', [
      '@amount' => commerce_currency_format($transaction->data['auth_amount'], $transaction->currency_code)
    ]),
    t('Order balance: @balance', [
      '@balance' => commerce_currency_format($balance['amount'], $balance['currency_code'])
    ]),
  ));

  $form['amount'] = array(
    '#type' => 'textfield',
    '#title' => t('Reversal amount'),
    '#description' => $description,
    '#default_value' => $default_amount,
    '#field_suffix' => check_plain($transaction->currency_code),
    '#size' => 16,
  );

  $form = confirm_form($form,
    t('What amount do you want to reverse?'),
    'admin/commerce/orders/' . $order->order_id . '/payment',
    '',
    t('Reverse'),
    t('Cancel'),
    'confirm'
  );

  return $form;
}

/**
 * Validate handler: ensure a valid amount is given.
 */
function commerce_vantiv_creditcard_reversal_form_validate($form, &$form_state) {
  $transaction = $form_state['transaction'];
  $amount = $form_state['values']['amount'];
  $card_type = !empty($transaction->data['card_type']) ? $transaction->data['card_type'] : FALSE;

  // Ensure a positive numeric amount has been entered for capture.
  if (!is_numeric($amount) || $amount <= 0) {
    form_set_error('amount', t('You must specify a positive numeric amount to capture.'));
  }

  // Ensure the amount is less than or equal to the authorization amount.
  if ($amount > commerce_currency_amount_to_decimal($transaction->data['auth_amount'], $transaction->currency_code)) {
    form_set_error('amount', t('You cannot reverse more than you authorized.'));
  }

  // If the authorization has expired, display an error message and redirect.
  if ($card_type && time() - $transaction->created > _commerce_vantiv_creditcard_authorization_lifespan($card_type)) {
    drupal_set_message(t('This authorization has passed its limit and the uncaptured balance has already been reversed.'), 'error');
    drupal_goto('admin/commerce/orders/' . $form_state['order']->order_id . '/payment');
  }
}

/**
 * Submit handler: process a prior authorization capture via AIM.
 */
function commerce_vantiv_creditcard_reversal_form_submit($form, &$form_state) {
  if (!commerce_vantiv_creditcard_load_library()) {
    return FALSE;
  }

  $payment_method = $form_state['payment_method'];
  $transaction = $form_state['transaction'];
  $order = $form_state['order'];

  // Format amount for different uses.
  $decimal_amount = number_format($form_state['values']['amount'], 2, '.', '');
  $amount = commerce_currency_decimal_to_amount($decimal_amount, $transaction->currency_code);
  $currency_amount = commerce_currency_format($amount, $transaction->currency_code);

  // Initialize request.
  $config = _commerce_vantiv_creditcard_get_config($payment_method);
  $request = new Vantiv\Request\Credit\Reversal($config);
  $request_body = [
    'Credentials' => [
      'AcceptorID' => _commerce_vantiv_creditcard_get_merchant_id($payment_method)
    ],
    'Reports' => [
      'ReportGroup' => _commerce_vantiv_creditcard_get_report_group($payment_method, $order)
    ],
    'Transaction' => [
      'TransactionID' => $transaction->remote_id,
      'TransactionAmount' => $decimal_amount,
    ],
    'Application' => [
      'ApplicationID' => $order->order_id . 'reversal'
    ],
  ];

  // Create a new transaction to record this reversal.
  $reversal_transaction = commerce_payment_transaction_new($payment_method['method_id'], $order->order_id);
  $reversal_transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;

  // Until COMMERCE_CREDIT_REVERSAL is a constant, use string.
  $reversal_transaction->remote_status = 'reversal';

  // Copy properties from authorization transaction.
  $reversal_transaction->instance_id = $transaction->instance_id;
  $reversal_transaction->currency_code = $transaction->currency_code;

  // Save reversal transaction so ID can be used in request_body.
  commerce_payment_transaction_save($reversal_transaction);

  // Append new transaction id to API request to ensure uniqueness.
  $request_body['Application']['ApplicationID'] .= $reversal_transaction->transaction_id;

  // Perform API request and prepare response.
  $response = $request->send($request_body);
  $response_body = $response['response']->getResponse();
  $request_id = $response['response']->getRequestID();
  $response_code = $response['http_code'];

  // Add log entry to transaction.
  $reversal_transaction->payload[$request_id] = _commerce_vantiv_creditcard_transaction_log($payment_method, $request, $request_body, $response, $response_body);

  // Set error messaging on failure or update transaction on success.
  if ($response_code !== 200 || !in_array($response_body->response, [000, 001, 010])) {
    drupal_set_message(t('Prior authorization reversal failed, so the reversal transaction will remain in a pending status.'), 'error');
    drupal_set_message(check_plain($response_body['message']), 'error');
  }
  else {
    drupal_set_message(t('Prior authorization reversed successfully.'));

    // Set transaction amount to zero since this reversal only affects
    // the authorization amount (potential to capture), and should not
    // affect the order balance.
    $reversal_transaction->amount = 0;

    // Update auth amount to reflect this reversal.
    // A non-zero value allows additional captures/reversals.
    $transaction->data['auth_amount'] -= $amount;

    // Set remote id with reversal transaction id.
    $reversal_transaction->remote_id = $response_body->TransactionID;

    // Set local and remote status.
    $reversal_transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;

    // Add reversal transaction event to authorization transaction message.
    $transaction->message .= '<br />' . t('(!txn) Reverse @amount (@date) (Bal: @bal)', [
      '!txn' => l($reversal_transaction->transaction_id, 'admin/commerce/orders/' . $order->order_id . '/payment/' . $reversal_transaction->transaction_id . '/view'),
      '@amount' => $currency_amount,
      '@date' => format_date(REQUEST_TIME, 'short'),
      '@bal' => commerce_currency_format($transaction->data['auth_amount'], $transaction->currency_code)
    ]);

    // Log reversal message on reversal transaction.
    $reversal_transaction->message = t('(!txn) Reverse @amount (@date)', [
      '!txn' => l($transaction->transaction_id, 'admin/commerce/orders/' . $order->order_id . '/payment/' . $transaction->transaction_id . '/view'),
      '@amount' => $currency_amount,
      '@date' => format_date(REQUEST_TIME, 'short')
    ]);

    // Set auth transaction status as success once depleted.
    if (empty($transaction->data['auth_amount'])) {
      $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
    }

    commerce_payment_transaction_save($transaction);
  }

  commerce_payment_transaction_save($reversal_transaction);

  $form_state['redirect'] = 'admin/commerce/orders/' . $form_state['order']->order_id . '/payment';
}

/**
 * Form callback: allows the user to credit a prior auth completion or sale.
 */
function commerce_vantiv_creditcard_credit_form($form, &$form_state, $order, $transaction) {
  $form_state['order'] = $order;
  $form_state['transaction'] = $transaction;

  // Load and store the payment method instance for this transaction.
  $payment_method = commerce_payment_method_instance_load($transaction->instance_id);
  $form_state['payment_method'] = $payment_method;

  // Convert the price amount to a user friendly decimal value.
  $amount = commerce_vantiv_creditcard_amount_to_decimal($transaction->amount, $transaction->currency_code);

  $form = confirm_form($form,
    t('Credit the order for the full amount (@amount)?', [
      '@amount' => $amount . ' ' . check_plain($transaction->currency_code)
    ]),
    'admin/commerce/orders/' . $order->order_id . '/payment',
    '',
    t('Credit'),
    t('Cancel'),
    'confirm'
  );

  return $form;
}

/**
 * Submit handler: process a credit transaction.
 */
function commerce_vantiv_creditcard_credit_form_submit($form, &$form_state) {
  $transaction = $form_state['transaction'];
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
  $request = new Vantiv\Request\Credit\Credit($config);
  $request_body = [
    'Credentials' => [
      'AcceptorID' => _commerce_vantiv_creditcard_get_merchant_id($payment_method)
    ],
    'Reports' => [
      'ReportGroup' => _commerce_vantiv_creditcard_get_report_group($payment_method, $order)
    ],
    'Transaction' => [
      'TransactionID' => $transaction->remote_id,
    ],
    'Application' => [
      'ApplicationID' => $order->order_id . 'credit'
    ],
  ];

  // Initialize a transaction to give us a new unique transaction id.
  // Vantiv requests should all contain a unique ApplicationID.
  $credit_transaction = commerce_payment_transaction_new($transaction->payment_method, $order->order_id);
  $credit_transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
  commerce_payment_transaction_save($credit_transaction);
  $request_body['Application']['ApplicationID'] .= $credit_transaction->transaction_id;

  // Perform request and prepare response.
  $response = $request->send($request_body);
  $response_body = $response['response']->getResponse();
  $request_id = $response['response']->getRequestID();
  $response_code = $response['http_code'];

  // Update and save the transaction based on the response.
  $credit_transaction->payload[$request_id] = _commerce_vantiv_creditcard_transaction_log($payment_method, $request, $request_body, $response, $response_body);

  // If we didn't get an approval response code...
  if ($response_code !== 200 || !in_array($response_body->response, [000, 001, 010])) {
    // Display an error message but leave the transaction pending.
    drupal_set_message(t('Credit failed.'), 'error');
    drupal_set_message(check_plain($response_body['message']), 'error');
  }
  else {
    drupal_set_message(t('Credit processed successfully.'));

    // Save the credit amount as a negative integer.
    $credit_transaction->amount = $transaction->amount * -1;

    $credit_transaction->currency_code = $transaction->currency_code;
    $credit_transaction->instance_id = $payment_method['instance_id'];
    $credit_transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
    $credit_transaction->remote_status = COMMERCE_CREDIT_CREDIT;
    $credit_transaction->remote_id = $response_body->TransactionID;
    $credit_transaction->message = implode('<br />', [
      $credit_transaction->status,
      t('Credited: @amount, @date', [
        '@amount' => commerce_vantiv_creditcard_amount_to_decimal($transaction->amount, $credit_transaction->currency_code),
        '@date' => format_date(REQUEST_TIME, 'short')
      ])
    ]);
  }

  commerce_payment_transaction_save($credit_transaction);

  $form_state['redirect'] = 'admin/commerce/orders/' . $form_state['order']->order_id . '/payment';
}

/**
 * Form callback: allows the user to void a credit transaction.
 */
function commerce_vantiv_creditcard_void_form($form, &$form_state, $order, $transaction) {
  $form_state['order'] = $order;
  $form_state['transaction'] = $transaction;

  // Load and store the payment method instance for this transaction.
  $payment_method = commerce_payment_method_instance_load($transaction->instance_id);
  $form_state['payment_method'] = $payment_method;

  // Convert the price amount to a user friendly decimal value.
  $amount = commerce_vantiv_creditcard_amount_to_decimal($transaction->amount, $transaction->currency_code);

  $form = confirm_form($form,
    t('Void the @txn of @amount?', [
      '@txn' => $transaction->remote_status,
      '@amount' => $amount . ' ' . check_plain($transaction->currency_code)
    ]),
    'admin/commerce/orders/' . $order->order_id . '/payment',
    '',
    t('Void'),
    t('Cancel'),
    'confirm'
  );

  return $form;
}

/**
 * Submit handler: process a vod transaction.
 */
function commerce_vantiv_creditcard_void_form_submit($form, &$form_state) {
  $transaction = $form_state['transaction'];
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
  $request = new Vantiv\Request\Credit\Void($config);
  $request_body = [
    'Credentials' => [
      'AcceptorID' => _commerce_vantiv_creditcard_get_merchant_id($payment_method)
    ],
    'Reports' => [
      'ReportGroup' => _commerce_vantiv_creditcard_get_report_group($payment_method, $order)
    ],
    'Transaction' => [
      'TransactionID' => $transaction->remote_id,
    ],
    'Application' => [
      'ApplicationID' => $order->order_id . 'void'
    ],
  ];

  // Initialize a transaction to give us a new unique transaction id.
  // Vantiv requests should all contain a unique ApplicationID.
  $void_transaction = commerce_payment_transaction_new($transaction->payment_method, $order->order_id);
  $void_transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
  commerce_payment_transaction_save($void_transaction);
  $request_body['Application']['ApplicationID'] .= $void_transaction->transaction_id;

  // Perform request and prepare response.
  $response = $request->send($request_body);
  $response_body = $response['response']->getResponse();
  $request_id = $response['response']->getRequestID();
  $response_code = $response['http_code'];

  // Update and save the transaction based on the response.
  // Add logging information to transaction.
  $void_transaction->payload[$request_id] = _commerce_vantiv_creditcard_transaction_log($payment_method, $request, $request_body, $response, $response_body);

  // If we didn't get an approval response code...
  if ($response_code !== 200 || !in_array($response_body->response, [000, 001, 010])) {
    // Display an error message but leave the transaction pending.
    drupal_set_message(t('Void failed.'), 'error');
    drupal_set_message(check_plain($response_body['message']), 'error');
  }
  else {
    drupal_set_message(t('Void processed successfully.'));

    // Save the void amount as a positive integer.
    $void_transaction->amount = $transaction->amount * -1;

    $void_transaction->currency_code = $transaction->currency_code;
    $void_transaction->instance_id = $payment_method['instance_id'];
    $void_transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
    $void_transaction->remote_status = COMMERCE_CREDIT_VOID;
    $void_transaction->remote_id = $response_body->TransactionID;
    $void_transaction->message = implode('<br />', [
      $void_transaction->status,
      t('Voided: @amount, @date', [
        '@amount' => commerce_vantiv_creditcard_amount_to_decimal($void_transaction->amount, $void_transaction->currency_code),
        '@date' => format_date(REQUEST_TIME, 'short')
      ])
    ]);
  }

  commerce_payment_transaction_save($void_transaction);

  $form_state['redirect'] = 'admin/commerce/orders/' . $form_state['order']->order_id . '/payment';
}
