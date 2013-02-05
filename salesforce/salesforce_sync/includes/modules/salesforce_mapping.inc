<?php

/**
 * @file
 * Extends salesforce_mapping functionality to work with salesforce_sync.
 */

/**
 * Implements hook_form_FORM_ID_alter().
 */
function salesforce_sync_form_salesforce_mapping_form_alter(&$form, &$form_state) {
  // Override validate and submit logic.
  $index = array_search('salesforce_mapping_form_validate', $form['#validate']);
  if ($index !== FALSE) {
    $form['#validate'] = array_replace($form['#validate'], array($index => 'sfsync_salesforce_mapping_salesforce_mapping_form_validate'));
  }
  $index = array_search('salesforce_mapping_form_submit', $form['#submit']);
  if ($index !== FALSE) {
    $form['#submit'] = array_replace($form['#submit'], array($index => 'sfsync_salesforce_mapping_salesforce_mapping_form_submit'));
  }

  // Add overwrite column.
  $form['salesforce_field_mappings_wrapper']['salesforce_field_mappings']['#theme'] = 'sfsync_salesforce_mapping_fieldmap_form_table';
  $mappings = is_object($form_state['salesforce_mapping']) ? $form_state['salesforce_mapping']->field_mappings : FALSE;
  $options = array(
    SALESFORCE_SYNC_RULE_NEVER => t('Never'),
    SALESFORCE_SYNC_RULE_BLANK => t('When blank'),
    SALESFORCE_SYNC_RULE_APPEND => t('Merge values'),
    SALESFORCE_SYNC_RULE_ALWAYS => t('Always'),
  );
  foreach (element_children($form['salesforce_field_mappings_wrapper']['salesforce_field_mappings']) as $key) {
    $form['salesforce_field_mappings_wrapper']['salesforce_field_mappings'][$key]['overwrite'] = array(
      '#type' => 'radios',
      '#options' => $options,
      '#default_value' => ($mappings && isset($mappings[$key]['overwrite'])) ? $mappings[$key]['overwrite'] : SALESFORCE_SYNC_RULE_NEVER,
    );
  }
}

/**
 * Validate callback salesforce_mapping_form().
 */
function sfsync_salesforce_mapping_salesforce_mapping_form_validate($form, &$form_state) {
  $values = $form_state['values'];
  $mapping = $form_state['salesforce_mapping'];

  // Validate label and name length.
  if (strlen($values['label']) > SALESFORCE_MAPPING_NAME_LENGTH) {
    form_set_error('label', t('Label must not exceed @max characters.', array('@max' => SALESFORCE_MAPPING_NAME_LENGTH)));
  }
  if (strlen($values['name']) > SALESFORCE_MAPPING_NAME_LENGTH) {
    form_set_error('name', t('Name must not exceed @max characters.', array('@max' => SALESFORCE_MAPPING_NAME_LENGTH)));
  }

  $efq = new EntityFieldQuery();
  $efq
  ->entityCondition('entity_type', 'salesforce_mapping')
  ->propertyCondition('salesforce_object_type', $values['salesforce_object_type'])
  ->propertyCondition('salesforce_record_type', $values['salesforce_record_type'])
  ->propertyCondition('drupal_entity_type', $values['drupal_entity_type'])
  ->propertyCondition('drupal_bundle', $values['drupal_bundle'])
  ->count();

  // Existing mapping, ensure not using any other unique combo.
  if ($mapping->name) {
    $efq->propertyCondition('name', $mapping->name, '<>');
  }

  $count = $efq->execute();
  if ($count > 0) {
    form_set_error('drupal_bundle', t('This Drupal bundle has already been mapped to a Salesforce object.'));
  }

  // If Salesforce Object Create is selected, ensure that at least one mapping
  // is set to sync or SF to Drupal to prevent "empty" entities.
  if ($values['sync_triggers'][SALESFORCE_MAPPING_SYNC_SF_CREATE] && isset($values['salesforce_field_mappings'])) {
    $valid_salesforce_create = FALSE;
    foreach ($values['salesforce_field_mappings'] as $mapping) {
      if ($mapping['direction'] == SALESFORCE_MAPPING_DIRECTION_SYNC || $mapping['direction'] == SALESFORCE_MAPPING_DIRECTION_SF_DRUPAL) {
        $valid_salesforce_create = TRUE;
        break;
      }
    }
    if (!$valid_salesforce_create) {
      form_set_error('mapping issue', t('One mapping must be set to "Sync" or "SF to Drupal" when "Salesforce object create" is selected'));
    }
  }
}

/**
 * Submit handler for salesforce_mapping_form().
 */
function sfsync_salesforce_mapping_salesforce_mapping_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $mappings = array();
  if (isset($values['key'])) {
    $key_delta = $values['key'];
    $sf_fields = $form_state['salesforce_mapping_object_' . $values['salesforce_object_type']]['fields'];
    foreach ($values['salesforce_field_mappings'] as $key => $mapping) {
      $drupal_field_type = $mapping['drupal_field']['fieldmap_type'];
      $field_map = salesforce_mapping_get_fieldmap_types($drupal_field_type);
      if (!empty($mapping['drupal_field']) && !empty($mapping['salesforce_field'])) {
        if (isset($field_map['salesforce_multiple_fields']) && $field_map['salesforce_multiple_fields']) {
          $sf_field = array();
          foreach ($mapping['salesforce_field'] as $field) {
            $sf_field[] = reset(array_filter($sf_fields, salesforce_field_filter($field)));
          }
        }
        else {
          $sf_field = reset(array_filter($sf_fields, salesforce_field_filter($mapping['salesforce_field'])));
        }

        $mappings[] = array(
          'drupal_field' => $mapping['drupal_field'],
          'salesforce_field' => $sf_field,
          'key' => $key_delta && ($key === (int) $key_delta),
          'direction' => $mapping['direction'],
          'overwrite' => $mapping['overwrite'],
        );
      }
    }
  }

  $sync_values = array_filter($values['sync_triggers']);
  $sync_triggers = SALESFORCE_MAPPING_SYNC_OFF;
  foreach ($sync_values as $value) {
    $sync_triggers = $sync_triggers | $value;
  }

  $params = array(
    'label' => $values['label'],
    'salesforce_object_type' => $values['salesforce_object_type'],
    'salesforce_record_type' => $values['salesforce_record_type'],
    'drupal_entity_type' => $values['drupal_entity_type'],
    'drupal_bundle' => $values['drupal_bundle'],
    'description' => '',
    'field_mappings' => $mappings,
    'sync_triggers' => $sync_triggers,
    'push_async' => $values['push_async'],
    'push_batch' => $values['push_batch'],
  );

  if ($form_state['salesforce_mapping']->name) {
    $mapping = $form_state['salesforce_mapping'];
    foreach ($params as $key => $value) {
      $mapping->{$key} = $value;
    }
  }
  else {
    $params['name'] = $values['name'];
    $mapping = entity_create('salesforce_mapping', $params);
  }

  $mapping->save();

  drupal_set_message(t('Salesforce field mapping saved.'));
  $form_state['redirect'] = 'admin/structure/salesforce/mappings';
}

/**
 * Implements hook_theme().
 */
function sfsync_salesforce_mapping_theme($existing, $type, $theme, $path) {
  return array(
    'sfsync_salesforce_mapping_fieldmap_form_table' => array(
      'render element' => 'elements',
    ),
  );
}

/**
 * Themes the field associations on a fieldmap edit form into a table.
 */
function theme_sfsync_salesforce_mapping_fieldmap_form_table($variables) {
  $elements = $variables['elements'];

  // Build the header array.
  $header = array(
    t('Drupal field'),
    t('Salesforce field'),
    t('key'),
    t('Direction'),
    t('Overwrite'),
    t('Delete'),
  );

  // Build the rows array.
  $rows = array();
  foreach (element_children($elements) as $child_key) {
    $child = & $elements[$child_key];
    $rows[] = array(
      array(
        'data' => drupal_render($child['drupal_field']),
      ),
      array(
        'data' => drupal_render($child['salesforce_field']),
      ),
      array(
        'data' => drupal_render($child['key']),
      ),
      array(
        'data' => drupal_render($child['direction']),
      ),
      array(
        'data' => drupal_render($child['overwrite']),
      ),
      array(
        'data' => drupal_render($child['delete_field_mapping']),
      ),
    );
  }

  return theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => $elements['#attributes'],
  ));
}