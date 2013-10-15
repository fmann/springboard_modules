<?php

/**
 * @file database helper functions.
 */

/**
 *  List master order ids in the import queue.
 */
function sustainer_import_list_master_order_ids() {
  $ids = array();
  $query = db_select('sustainer_import_queue', 's');
  $query->fields('s', array('master_order_id'));
  $results = $query->execute();
  while ($id = $results->fetchField()) {
    $ids[] = $id;
  }
  return $ids;
}

/**
 * Update record status in import queue.
 *
 * @param $master_order_id
 * Original master order id of the sustainer series record.
 *
 * @param $status
 * New status to assign to this record.
 */
function sustainer_import_flag_record($master_order_id, $status) {
  db_query('UPDATE {sustainer_import_queue} SET status = :status WHERE master_order_id = %d', array(':status' => $status, ':id' => $master_order_id));
}

/**
 * Save an entry to the import queue.
 *
 * @param $record
 * sustainer record
 *
 */
function sustainer_import_import_record_insert($record) {
  $master_ids = sustainer_import_list_master_order_ids();

  if (in_array($record['order_id'], $master_ids)) {
    return FALSE;
  }
  dsm($record, 'record during save');
  $data = array(
    'master_order_id' => $record['order_id'],
    'record' => serialize($record),
    'created' => time(),
    'status' => '',
    'gateway_type' => $record['payment_details']['gateway_type'],
  );

  $output = drupal_write_record('sustainer_import_queue', $data);
  dsm($output, 'dwr output');
}

function sustainer_import_get_pending_queue_items() {
  $records = array();
  $results = db_query("SELECT record FROM {sustainer_import_queue} WHERE status = ''");
  while ($item = $results->fetchAssoc()) {
    $records[] = unserialize($item['record']);
  }
  return $records;
}

/**
 * Provide a count of queue items by status.
 *
 * @return array
 */
function sustainer_import_queue_status_counts() {
  $output = array();
  $results = db_query('SELECT status as status, COUNT(status) AS count FROM {sustainer_import_queue} GROUP BY status');
  while ($data =  $results->fetchAssoc()){
     $output[] = $data;
  }
  return $output;
}

/**
 * List payment gateway types in queue.
 */
function sustainer_import_queue_gateway_types() {
  $output = array();
  $results = db_query('SELECT DISTINCT(gateway_type) FROM {sustainer_import_queue}');
  while ($data = $results->fetchAssoc()) {
    $output[] = $data['gateway_type'];
  }
  return $output;
}

/**
 * List available payment gateways.
 */
function sustainer_import_list_payment_gateways() {
  $gateway_info = _fundraiser_gateway_info();
  foreach ($gateway_info as $key => $gateway) {
    $gateways[$key] = $gateway['title'];
  }
  dsm($gateways);
  return $gateways;
}
