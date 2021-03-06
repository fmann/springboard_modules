<?php

/**
 * @file
 * Commerce based hook for commerce_paypal.
 */

/**
 * Implements hook_fundraiser_commerce_fundraiser_gateway_info().
 */
function commerce_paypal_wps_fundraiser_commerce_fundraiser_gateway_info() {
  return array(
    'payment_method' => array('paypal'),
    'offsite_processing' => array('paypal'),
    'form callback' => 'commerce_paypal_wps_fundraiser_commerce_form',
    'redirect callback' => 'commerce_paypal_wps_fundraiser_commerce_redirect',
    'scrub callback' => 'commerce_paypal_wps_fundraiser_commerce_scrub',
    // Validation callback is commerce_paypal_wps_fundraiser_commerce_redirect_validate
    // Submit callback is commerce_paypal_wps_fundraiser_commerce_redirect_submit
  );
}

/**
 * Callback function, use this form for the given gateway.
 */
function commerce_paypal_wps_fundraiser_commerce_form($payment_method, $config) {
  $method_instance = commerce_payment_method_instance_load($config['id']);
  $form = array();

  if (!empty($method_instance['settings']['show_payment_instructions'])) {
    $form['paypal_wps_information'] = array(
      '#markup' => '<span class="commerce-paypal-wps-info">' . t('After submitting the form you will be taken to PayPal to enter your payment details.') . '</span>',
    );
  }

  return $form;
}

/**
 * Callback function, handle the redirect form.
 */
function commerce_paypal_wps_fundraiser_commerce_redirect($method_instance, $donation, $order, $settings) {
  $form = drupal_get_form('commerce_paypal_wps_order_form', $order, $method_instance['settings'] + $settings);

  // Pass these fields to prepopulate the paypal fields: https://www.paypal.com/cgi-bin/webscr?cmd=_pdn_xclick_prepopulate_outside
  $paypal_fields = array(
    'mail' => 'email',
    'first_name',
    'last_name',
    'address' => 'address1',
    'address_line_2' => 'address2',
    'city',
    'state',
    'zip',
  );
  // Pull the data from the donation object
  foreach ($paypal_fields as $key => $value) {
    // If the key is a string use it as the donation array key
    $donor_key = is_string($key) ? $key : $value;
    if (!empty($donation->donation[$donor_key])) {
      $form[$value] = array(
        '#type' => 'hidden',
        '#value' => $donation->donation[$donor_key],
      );
    }
  }

  return $form;
}

/**
 * Callback function, validate after redirect.
 */
function commerce_paypal_wps_fundraiser_commerce_redirect_validate($method_instance, $donation, $order) {
  $result = commerce_paypal_wps_redirect_form_validate($order, $method_instance);
  $success = FALSE;
  if ($result !== FALSE) {
    $success = TRUE;
  }
  return $success;
}

/**
 * Callback function, scrub the data before saving.
 */
function commerce_paypal_wps_fundraiser_commerce_scrub($submission_fields) {
  // Nothing to store for paypal
  return array();
}

/**
 * Callback function, submit after redirect.
 */
function commerce_paypal_wps_fundraiser_commerce_redirect_submit($method_instance, $donation, $order) {
  // IMPORTANT: Transaction handling for Paypal is handled on a differnt URL, when Paypal returns the IPN tracsaction ID.
  // Therefore no additional transaction work occurs here.
  return TRUE;
}
