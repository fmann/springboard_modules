<?php

/**
 * @file
 * Definition of views_handler_filter_node_type.
 */ 

/**
 * Filter by node type.
 *
 * @ingroup views_filter_handlers
 */
class salesforce_log_handler_field_drupal_id extends views_handler_field_numeric {
  function render($values) {
    $value = $output = $values->{$this->field_alias};
    // If the module field is present link this id to the drupal object
    $module_field_alias = $this->view->base_table . '_module';
    if (!empty($values->{$module_field_alias})) {
      switch ($values->{$module_field_alias}) {
        case 'webform':
          // Need the nid to link to the webform submission
          $nid = db_query('SELECT nid FROM webform_submissions WHERE sid = :sid', array(':sid' => $value))->fetchField();
          if ($nid) { 
            $output = l($value, 'node/' . $nid . '/submission/' . $value);
          }
          break;
        case 'user':
          $output = l($value, 'user/' . $value);
          break;
      }
    }

    return $output;
  }
}