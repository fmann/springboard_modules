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
    'allow_recurring' => FALSE,
    // ALL currently supported ubercart gateways use the same field set.
    'form callback' => 'commerce_paypal_wps_fundraiser_commerce_paypal_form',
    //$this_gateway['form validate callback'] = '',
    //$this_gateway['form submit callback'] = '',
  );
}

/**
 * Callback function, use this form for the given gateway.
 */
function commerce_paypal_wps_fundraiser_commerce_paypal_form($payment_method) {
  $form['paypal_email'] = array(
    '#type' => 'textfield',
    '#title' => t('Paypal email'),
  );
  return $form;
}

/**
 * Implements hook_fundraiser_commerce_execute_gateway().
 */
function commerce_paypal_wps_fundraiser_commerce_execute_gateway($payment_method, $donation) {
  drupal_goto('fundraiser-commerce/redirect/' . $donation->did);
  // Results and returns to be handled after the redirect return.
}

/**
 * Implements hook_fundraiser_commerce_redirect_form().
 */
function commerce_paypal_wps_fundraiser_commerce_redirect_form($form, &$form_state, $order, $payment_method) {
  $settings = array(
    // Return to the previous page when payment is canceled
    'cancel_return' => url('fundraiser-commerce/redirect/' . $order->order_id . '/back/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),

    // Return to the payment redirect page for processing successful payments
    'return' => url('fundraiser-commerce/redirect/' . $order->order_id . '/return/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),

    // Specify the current payment method instance ID in the notify_url
    'payment_method' => $payment_method['instance_id'],
  );

  return commerce_paypal_wps_order_form($form, $form_state, $order, $payment_method['settings'] + $settings);
}

/**
 * Implements hook_fundraiser_commerce_scrub_payment_fields().
 */
function commerce_paypal_wps_fundraiser_commerce_scrub_payment_fields($payment_method, $fields) {
  return $fields;
}