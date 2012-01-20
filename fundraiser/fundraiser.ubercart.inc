<?php

/**
 * @file
 * Ubercart based hook implementations for the fundraiser module.
 * These functions are split off with the idea that eventually this could become it's own module, linking fundraiser and ubercart.
 */

/**
 * Implementation of hook_order_actions() from Ubercart.
 * Add a refund option to the Order list
 * From http://drupalcontrib.org/api/drupal/contributions!ubercart!docs!hooks.php/function/hook_order_actions/6
 */
function fundraiser_order_actions($order) {
  if (user_access('edit orders') && (in_array($order->order_status, array('payment_received', 'partially_refunded')) || empty($order->order_status)) && fundraiser_gateway_can_refund_order($order)) {
    $module_path = base_path() . drupal_get_path('module', 'fundraiser');
    $title = t('Issue refund on order !order_id.', array('!order_id' => $order->order_id));
    $actions[] = array(
      'name' => t('Refund'),
      'url' => 'admin/store/orders/' . $order->order_id . '/refund',
      'icon' => '<img src="' . $module_path . '/images/refund.gif" alt="' . $title . '" />',
      'title' => $title,
    );
    return $actions;
  }
}

/**
 * Implementation of hook_order().
 * From http://www.ubercart.org/docs/api/hook_order
 * When an order is deleted, keep fundraiser tables in sync.
 */
function fundraiser_order($op, &$arg1, $arg2) {
  switch ($op) {
    case 'delete':
      db_delete('fundraiser_recurring')->condition('order_id', $arg1->order_id)->execute();
      break;
  }
}

/**
 * Implementation of hook_preprocess_uc_order().
 * From: http://www.ubercart.org/forum/development/18047/inserting_variables_invoices_ubercart_23_and_above
 * Adds additional variables for use in donation receipt emails sent by UC.
 */
function fundraiser_preprocess_uc_order(&$variables) {
  switch ($variables['op']) {
    case 'checkout-mail':
      $nid = $variables['order']->products[0]->nid;
      $message = db_query("SELECT receipt_email_message FROM {fundraiser} WHERE nid = :nid", array(':nid' => $nid))->fetchColumn();
      $variables['fundraiser_message'] = $message;
      break;
  }
}

/**
 * Sage billing information update function.
 */
function sage_update_billing_information($reference_id, $billing_info) {
  // TODO hook here to load up order info. - SeH 
  $vault_id = db_query("SELECT vault_id from {uc_sage_vault} WHERE guid = :guid", array(':guid' => $reference_id))->fetchColumn();
  $data = array(
    'billing_name' => $billing_info['first_name'] . ' ' . $billing_info['last_name'],
    'billing_street1' => $billing_info['address_1'],
    'billing_street2' => $billing_info['address_2'],
    'billing_city' => $billing_info['city'],
    'billing_zone' => $billing_info['state'],
    'billing_postal_code' => $billing_info['zipcode'],
    'billing_country' => $billing_info['country'],
    'cc_number' => $billing_info['card_num'],
    'cc_exp_month' => sprintf("%02d", $billing_info['card_exp_date']['month']),
    'cc_exp_year' => $billing_info['card_exp_date']['year'],
  );
  $response = uc_sage_vault_update($vault_id, $data);
  // TODO: Need to add some better returns to the uc_sage module
  return TRUE;
}

/**
 * Implements hook_fundraiser_get_user_country().
 */
function fundraiser_fundraiser_get_user_country($profile_field_id, $uid) {
  return db_query('SELECT uc.country_id FROM {uc_countries} uc '.
    'INNER JOIN {profile_value} pv ON pv.value = uc.country_iso_code_2 AND pv.uid = :uid AND pv.fid = :fid',
    array(':uid' => $uid, ':fid' => $profile_field_id))->fetchColumn();
}

/**
 * Implements hook_fundraiser_get_user_zone().
 */
function fundraiser_fundraiser_get_user_zone($profile_field_id, $uid) {
  return db_query('SELECT uz.zone_id FROM {uc_zones} uz '.
    'INNER JOIN {profile_value} pv ON pv.value = uz.zone_code AND pv.uid = :uid AND pv.fid = :fid',
    array(':uid' => $uid, ':fid' => $profile_field_id))->fetchColumn();
}

/**
 * Implements hook_fundraiser_user_has_recurring_orders().
 */
function fundraiser_fundraiser_user_has_recurring_orders($uid) {
  $count = db_query('SELECT COUNT(u.order_id) FROM uc_orders u '.
    'INNER JOIN fundraiser_recurring f ON f.master_order_id = u.order_id '.
    'WHERE u.uid = :uid ',
    array(':uid' => $uid))->fetchColumn();
  if ($count > 0) {
    return TRUE;
  }
  return FALSE;
}

/**
 * Implements hook_fundraiser_user_get_recurring_orders().
 */
function fundraiser_fundraiser_user_get_recurring_orders($uid) {
  return db_query('SELECT f.order_id, f.recurring_status FROM {fundraiser_webform_order} f '.
    'INNER JOIN {uc_orders} u ON u.order_id = f.order_id '.
    'WHERE f.recurring_status != :status AND u.uid = :uid',
    array(':status' => DONATION_NON_RECURRING, ':uid' => $uid));
}

/**
 * Implements hook_fundraiser_user_get_recurring_order_schedule().
 */
function fundraiser_fundraiser_user_get_recurring_order_schedule($master_order_id) {
  return db_query('SELECT u.order_total AS amount, f.next_charge AS charge_date, f.gateway_resp AS status FROM {fundraiser_recurring} f '.
    'INNER JOIN {uc_orders} u ON u.order_id = f.order_id '.
    'WHERE f.master_order_id = :order_id ' .
    'ORDER BY f.next_charge ASC',
    array(':order_id' => $master_order_id));
}

/**
 * Implements hook_fundraiser_get_current_donation_amount().
 */
function fundraiser_fundraiser_get_current_donation_amount($master_order_id) {
  return db_query('SELECT u.order_total FROM {uc_orders} u '. 
    'INNER JOIN {fundraiser_recurring} f ON f.order_id = u.order_id '.
    'WHERE (f.gateway_resp IS NULL OR f.gateway_resp = \'canceled\') AND f.master_order_id = :order_id '.
    'ORDER BY next_charge LIMIT 1',
    array(':order_id' => $order_id))->fetchColumn();
}

/**
 * Implements hook_fundraiser_get_order_start().
 */
function fundraiser_fundraiser_get_order_start($order_id) {
  return db_query('SELECT created FROM {uc_orders} WHERE order_id = :order_id',
    array(':order_id' => $order_id))->fetchColumn();
}

/**
 * Implements hook_fundraiser_get_country_zones().
 */
function fundraiser_fundraiser_get_country_zones($country_id) {
  $options = array('required' => uc_address_field_required('zone'));
  $zones = uc_zone_select(uc_get_field_name('zone'), NULL, $country_id, $options);
  $zone_array = array();
  foreach ($zones['#options'] as $key => $value) {
    $zone_array[$key] = $value;
  }
  return $zone_array;
}

/**
 * Implements hook_fundraiser_get_country_zone().
 */
function fundraiser_fundraiser_get_country_zone($zone_id) {
  return uc_get_zone_code($zone_id);
}

/**
 * Implements hook_fundraiser_get_country_iso().
 */
function fundraiser_fundraiser_get_country_iso($country_code) {
  return db_query('SELECT country_iso_code_2 FROM {uc_countries} WHERE country_id = :country_id',
    array(':country_id' => $country_code))->fetchField();
}

/**
 * Implements hook_fundraiser_get_country_zone().
 */
function fundraiser_fundraiser_get_zone_iso($country_code, $state_code) {
  return db_query('SELECT zone_code FROM {uc_zones} WHERE zone_id = :zone_id AND zone_country_id = :zone_country_id',
    array(':zone_id' => $state_code, ':zone_country_id' => $country_code))->fetchField();
}
/**
 * Implements hook_fundraiser_gateway_info().
 */
function fundraiser_fundraiser_gateway_info() {
  return _uc_payment_gateway_list('', TRUE);
// From Ubsercart http://api.lullabot.com/_uc_payment_gateway_list/7
// Is ultimately configured by http://api.lullabot.com/hook_uc_payment_gateway
// Returns in the format:
// $gateway = array('id', 'title', 'credit', 'credit_refund', 'etc');
}

/**
 * Implements hook_fundraiser_get_credit_encryption_path().
 */
function fundraiser_fundraiser_get_credit_encryption_path() {
  return variable_get('uc_credit_encryption_path', t('Not configured, see below.'));
}

/**
 * Implements hook_fundraiser_decrypt_credit().
 */
function fundraiser_fundraiser_decrypt_credit($data) {
  $key = uc_credit_encryption_key();
  $crypt = new uc_encryption_class;
  $cc = unserialize($data);
  $cc_data = unserialize($crypt->decrypt($key, $cc['cc_data']));
  $cc_data['cc_number'] = substr($cc_data['cc_number'], -4);
  return $cc_data;
}

/**
 * Implements hook_fundraiser_validate_card().
 */
function fundraiser_fundraiser_validate_card($cc_number){
  if(variable_get('uc_credit_validate_numbers', TRUE)) {
    return _uc_credit_valid_card_number($cc_number);
  }
  return FALSE;
}

/**
 * Implements hook_fundraiser_validate_card_expiration().
 */
function fundraiser_fundraiser_validate_card_expiration($cc_expiration_month, $cc_expiration_year){
  return _uc_credit_valid_card_expiration($cc_expiration_month, $cc_expiration_year);
}


/**
 * Implements hook_fundraiser_validate_card_cvv().
 */
function fundraiser_fundraiser_validate_card_cvv($cc_cvv){
  if (variable_get('uc_credit_cvv_enabled', TRUE)) {
    return _uc_credit_valid_cvv($cc_cvv);
  }
  return FALSE;
}

/**
 * Implements hook_fundraiser_create_order().
 */
function fundraiser_fundraiser_create_order($user, $fundraiser_fields, $recurs, $additional_order_fields) {
  // pick out the values we need to generate an order
  $donation_amount = $fundraiser_fields['amount'];
  $cc_number = $fundraiser_fields['card_number'];
  $cc_cvv = $fundraiser_fields['card_cvv'];
  $cc_expiration_month = $fundraiser_fields['card_expiration_month'];
  $cc_expiration_year = $fundraiser_fields['card_expiration_year'];
  $first_name = $fundraiser_fields['first_name'];
  $last_name = $fundraiser_fields['last_name'];
  $email = $fundraiser_fields['email'];
  $billing_address = $fundraiser_fields['address'];
  $billing_address_2 = $fundraiser_fields['address_line_2'];
  $billing_city = $fundraiser_fields['city'];
  $billing_country = $fundraiser_fields['country'];
  $billing_state = $fundraiser_fields['state'];
  $billing_zipcode = $fundraiser_fields['zip'];
  $quantity = $fundraiser_fields['quantity'];

  // Look for other amount if set.
  if ($donation_amount == "other") {
    $donation_amount = preg_replace("/[^\d\.]/i", "", $fundraiser_fields['other_amount']);
  }

  // Create an UC order.
  $order = uc_order_new($user->uid);
  $order->products[0] = uc_product_load($node);
  $order->products[0]->price = $donation_amount;
  $order->products[0]->qty = empty($quantity) ? 1 : $quantity;
  $order->products[0]->title = $node->title;
  $order->products[0]->nid = $node_id;
  $order->products[0]->data = array(
    'shippable' => $order->products[0]->shippable,
    'model' => $order->products[0]->model,
    'varprice' => $donation_amount,
    'module' => 'uc_product',
  );

  // Multiply amount by quantity if available.
  if (!empty($quantity)) {
    $donation_amount = $donation_amount * $quantity;
  }

  // Set order properties.
  $order->primary_email = $email;
  $order->order_total = $donation_amount;
  $order->billing_first_name = $first_name;
  $order->billing_last_name = $last_name;
  $order->billing_city = $billing_city;
  $order->billing_street1 = $billing_address;
  $order->billing_street2 = $billing_address_2;
  $order->billing_postal_code = $billing_zipcode;
  $order->billing_zone = $billing_state;
  $order->billing_country = $billing_country;
  $order->payment_method = 'credit';
  $order->payment_details = array(
    'cc_type' => _fundraiser_get_cc_type($cc_number),
    'cc_owner' => '',
    'cc_number' => $cc_number,
    'cc_start_month' => '',
    'cc_start_year' => '',
    'cc_exp_month' => $cc_expiration_month,
    'cc_exp_year' => $cc_expiration_year,
    'cc_issue' => '',
    'cc_cvv' => $cc_cvv,
    'cc_bank' => '',
  );
  $order->line_items = array();
  $order->data['recurring_status'] = $recurs;
  foreach ($additional_order_fields as $field => $value) {
    $order->data[$field] = $value;
  }

  // If using the gateway manager, store the gateway that was used to make the payment
  if (module_exists('gateway_manager')) {
    $order->data['gateway'] = $node->gateway;
  }

  // cache the cc details stored by the handler.
  uc_credit_cache('save', $order->payment_details, FALSE);
  // save the order
  uc_order_save($order);
  return $order->order_id;
}

/**
 * Implements hook_fundraiser_get_order().
 */
function fundraiser_fundraiser_get_order($order_id) {
  return uc_order_load($order_id);
}

/**
 * Implements hook_fundraiser_update_order_amount().
 */
function fundraiser_fundraiser_update_order_amount($order_id, $amount, $uid) {
  $order = uc_order_load($order_id);
  $old_amount = $order->order_total;
  $order->order_total = $amount;
  $order->products[0]->price = $amount;
  uc_order_save($order);
  uc_order_comment_save($order->order_id, $uid, "Donation amount changed from
    $old_amount to $amount by an administrator.", 'admin');
}

/**
 * Implements hook_fundraiser_update_order_comment().
 */
function fundraiser_fundraiser_update_order_comment($order_id, $message, $uid) {
  uc_order_comment_save($order_id, 0, $message, 'admin');
}

/**
 * Implements hook_fundraiser_update_order_billing().
 */
function fundraiser_fundraiser_update_order_billing( $order_id, $form_state) {
  global $user;
  $order = uc_order_load($order_id);
  $order->primary_email = $form_state['values']['email'];
  $order->billing_first_name =  $form_state['values']['first_name'];
  $order->billing_last_name = $form_state['values']['last_name'];
  $order->billing_city = $form_state['values']['city'];
  $order->billing_street1 = $form_state['values']['address_1'];
  $order->billing_street2 = $form_state['values']['address_2'];
  $order->billing_postal_code = $form_state['values']['zipcode'];
  $order->billing_zone = $form_state['values']['state'];
  $order->billing_country = $form_state['values']['country'];
  $order->payment_method = 'credit';
  $order->payment_details = array(
    'cc_type' => _fundraiser_get_cc_type($form_state['values']['card_num']),
    'cc_owner' => '',
    'cc_number' => $form_state['values']['card_num'],
    'cc_start_month' => '',
    'cc_start_year' => '',
    'cc_exp_month' => $form_state['values']['card_exp_date']['month'],
    'cc_exp_year' => $form_state['values']['card_exp_date']['year'],
    'cc_issue' => '',
    'cc_cvv' => $form_state['values']['card_cvv'],
    'cc_bank' => '',
  );
  uc_credit_cache('save', $order->payment_details, FALSE);
  uc_order_save($order);
  uc_credit_cache('clear');
  uc_order_comment_save($order->order_id, $user->uid, "Billing information updated.");
  return $order;
}

/**
 * Implements hook_fundraiser_get_order_paid().
 */
function fundraiser_fundraiser_get_order_paid($order_id) {
  return db_query('SELECT SUM(amount) FROM {uc_payment_receipts} WHERE order_id = :order_id',
    array(':order_id' => $order_id))->fetchColumn();
}

/**
 * Implements hook_fundraiser_get_order_payment_info().
 */
function fundraiser_fundraiser_get_order_payment_info($order_id) {
  $payment_info = db_query('SELECT u.receipt_id, u.amount, u.received, f.txn_id FROM {uc_payment_receipts} u ' .
    'LEFT JOIN {fundraiser_webform_order} f ON u.order_id = f.order_id ' .
    'WHERE u.amount > 0 AND u.order_id = :order_id', array(':order_id' => $order_id))->fetchAssoc();

  // If not payment info was found, check if it was a recurring order
  if (!$payment_info) {
    $payment_info = db_query('SELECT u.receipt_id, u.amount, u.received, f.txn_id FROM {uc_payment_receipts} u ' .
      'LEFT JOIN {fundraiser_recurring} f ON u.order_id = f.order_id ' .
      'WHERE u.amount > 0 AND u.order_id = :order_id', array(':order_id' => $order_id))->fetchAssoc();
  }
  return $payment_info;
}

/**
 * Implements hook_fundraiser_get_order_refund_info().
 */
function fundraiser_fundraiser_get_order_refund_info($order_id) {
  return db_query('SELECT SUM(amount) FROM {uc_payment_receipts} '.
    'WHERE order_id = :order_id AND amount < 0', array(':order_id' => $order_id))->fetchColumn();
}

/**
 * Implements hook_fundraiser_order_success().
 */
function fundraiser_fundraiser_order_success($order_id) {
  // Complete the sale in Ubercart.
  $order = uc_order_load($order_id);
  uc_cart_complete_sale($order, variable_get('uc_new_customer_login', FALSE));
  uc_payment_enter($order_id, 'fundraiser', $order->order_total, 0, NULL, t('Payment processed by the fundraiser module.'));
  db_query("UPDATE {uc_orders} SET order_status = 'payment_received' WHERE order_id = :order_id", array(':order_id' => $order_id));
  // And done.
}

/**
 * Implements hook_fundraiser_create_future_orders().
 */
function fundraiser_fundraiser_create_future_orders($user, $order_id, $gateway) {
  $orders = array();
  // Determine the number of future donations (Salesforce hard limit is 50. Set as 49 because original order is 1)
  $counter = min(_fundraiser_months_between_dates($order->payment_details['cc_exp_month'], $order->payment_details['cc_exp_year']), 49);
  for ($i = 1; $i <= $counter; $i++) {
    $next_order = uc_order_new($user->uid);
    $new_id = $next_order->order_id;
    $next_charge = strtotime("+$i months");
    $orders[] = array(
      'order_id' => $order_id,
      'new_id' => $new_id,
      'next_charge' => $next_charge,
      'gateway' => $gateway,
    );

    // Copy all values to new order
    $next_order = clone $order;
    $next_order->order_id = $new_id;
    $next_order->order_status = 'pending_future_payment';
    unset($next_order->products[0]->order_product_id);
    uc_order_save($next_order);
    // Add a comment to the order stating when it will be charged.
    uc_order_comment_save($new_id, 0, t('Payment will be processed on !date.', array('!date' => date('n/j/Y', $next_charge))), 'admin');
  }
  // Return newly created order set to fundraiser.
  return $orders;
}

/**
 * Implements hook_fundraiser_cancel_future_orders().
 */
function fundraiser_fundraiser_cancel_future_orders($master_order_id, $reason, $user) {
  // get the ids of the child orders
  $orders = _get_pending_recurring_orders($master_order_id);
  db_query('UPDATE {uc_orders} u ' .
    'INNER JOIN {fundraiser_recurring} f ON f.order_id = u.order_id '.
    'SET u.order_status = \'canceled\' ' .
    'WHERE f.master_order_id = :order_id and u.order_status = \'pending_future_payment\'',
    array(':order_id' => $master_order_id));

  $order_ids = array();
  // add a comment to the canceled orders
  foreach ($orders as $data) {
    $order_ids[] = $data->order_id;
    uc_order_comment_save($data->order_id, $user->uid, 'Order canceled by an administrator. Reason: ' . $reason, 'order', 'canceled', FALSE);
  }
  return $order_ids;
}

/**
 * Implements hook_fundraiser_order_decline().
 */
function fundraiser_fundraiser_order_decline($order_id) {
  // Mark the sale in Ubercart as failed.
  db_query("UPDATE {uc_orders} SET order_status = 'failed' WHERE order_id = :order_id", array(':order_id' => $order_id));
  // Clear the credit card cache between orders
  uc_credit_cache('clear');
}

/**
 * Implements hook_fundraiser_order_exception().
 */
function fundraiser_fundraiser_order_exception($order_id) {
  return fundraiser_fundraiser_order_decline($order_id);
}

/**
 * Implements hook_fundraiser_order_cancel().
 */
function fundraiser_fundraiser_order_cancel($order_id) {
  // Mark the sale in Ubercart as cencelled.
  db_query("UPDATE {uc_orders} SET order_status = 'canceled' WHERE order_id = :order_id", array(':order_id' => $order_id));
  // Make a comment on the order.
  uc_order_comment_save($order_id, 0, t('Payment was canceled during checkout by the user.'));
}

/**
 * Implements hook_fundraiser_order_refund().
 */
function fundraiser_fundraiser_order_refund($order_id, $amount, $new_status, $transaction_id, $reason) {
  $data = array('txn_type' => UC_CREDIT_CREDIT, 'txn_id' => $transaction_id);
  uc_payment_enter($order_id, 'fundraiser', $amount, 0, $data, 'Refund processed by the fundraiser module.');
  // Make the charge amount positive again, for nice display in the admin comment
  $amount = abs($payment_info['amount']);
  $reason = check_plain($reason);
  $amount_formatted = module_invoke_all('fundraiser_format_price', $amount);
  global $user;
  uc_order_comment_save($payment_info['order_id'], 
    $user->uid,
    t('%amount refund issued by %user. Refund notes: %reason',
      array('%amount' => $amount_formatted, '%user' => $user->name, '%reason' => $reason)),
    'admin');
  // Set order status to Refunded or Partially Refunded
  db_query("UPDATE {uc_orders} SET order_status= :order_status where order_id = :order_id",
    array(':order_status' => $new_status, ':order_id' => $order_id));

  // get the receipt_id that belongs to this refund
  $receipt_id = NULL;
  $payments = uc_payment_load_payments($order_id);
  foreach ($payments as $payment) {
    $data = unserialize($payment->data);
    if ($data['txn_id'] == $result['data']['txn_id']) {
      $receipt_id = $payment->receipt_id;
    }
  }
  return $receipt_id;
}

/**
 * Implements hook_fundraiser_monthly_report().
 */
// TODO consider if we can use views to create this report?
// This is massive and ugly and *really* should be handled some other way.
function fundraiser_fundraiser_monthly_report($start_date, $end_date) {
  $sql = "SELECT
            u.order_id,
            u.uid,
            u.order_status,
            s.sfid,
            u.billing_first_name,
            u.billing_last_name,
            u.primary_email,
            u.billing_street1,
            u.billing_street2,
            u.billing_city,
            u.data,
            z.zone_code,
            u.billing_postal_code,
            c.country_iso_code_2,
            u.created as date,
            u.order_total,
            p.received,
            op.nid,
            op.title,
            fd.campaign_id,
            fd.market_source
          FROM
            uc_orders u INNER JOIN fundraiser_webform_order s ON s.order_id = u.order_id
            INNER JOIN {uc_payment_receipts} p ON p.order_id = u.order_id
            INNER JOIN {uc_countries} c ON c.country_id = u.billing_country
            INNER JOIN {uc_zones} z ON z.zone_id = u.billing_zone
            INNER JOIN {uc_order_products} op ON op.order_id = u.order_id
            INNER JOIN {fundraiser_webform_order} wo ON wo.order_id = u.order_id
            INNER JOIN {fundraiser_webform_data} fd ON fd.sid = wo.sid AND fd.nid = wo.webform_nid
            WHERE u.order_status = 'payment_received' AND u.created BETWEEN :start_date AND :end_date 
          UNION SELECT
            r.order_id,
            u.uid,
            u.order_status,
            r.sf_opportunity_id,
            u.billing_first_name,
            u.billing_last_name,
            u.primary_email,
            u.billing_street1,
            u.billing_street2,
            u.billing_city,
            u.data,
            z.zone_code,
            u.billing_postal_code,
            c.country_iso_code_2,
            next_charge,
            u.order_total,
            p.received,
            op.nid,
            op.title,
            fd.campaign_id,
            fd.market_source
          FROM fundraiser_recurring r INNER JOIN uc_orders u ON u.order_id = r.order_id
            INNER JOIN {uc_payment_receipts} p ON p.order_id = u.order_id
            INNER JOIN {uc_countries} c ON c.country_id = u.billing_country
            INNER JOIN {uc_zones} z ON z.zone_id = u.billing_zone
            INNER JOIN {uc_order_products} op ON op.order_id = u.order_id
            INNER JOIN {fundraiser_webform_recurring_order} wo ON wo.order_id = u.order_id
            INNER JOIN {fundraiser_webform_data} fd ON fd.sid = wo.sid AND fd.nid = wo.nid
            WHERE u.order_status = 'payment_received' AND r.next_charge BETWEEN :start_date AND :end_date ORDER BY order_id";
  return db_query($sql, array(':start_date' => $start_date, ':end_date' => $end_date));
}

// Theme / formaty code, really should get refactored at some point. -SeH

/**
 * Implements hook_fundraiser_format_price().
 */
function fundraiser_fundraiser_format_price($price) {
  return theme('uc_price', array('price' => $price));
}

/**
 * Implements hook_fundraiser_format_country_select().
 */
function fundraiser_fundraiser_format_country_select($country) {
  return uc_country_select(uc_get_field_name('country'), $country, NULL, 'name', uc_address_field_required('country'));
}

/**
 * Implements hook_fundraiser_format_zone_select().
 */
function fundraiser_fundraiser_format_zone_select($country, $zone) {
  return uc_zone_select(uc_get_field_name('zone'), $zone, NULL, $country, 'name', uc_address_field_required('zone'));
}