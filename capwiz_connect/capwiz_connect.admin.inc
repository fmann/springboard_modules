<?php
/**
 * @file
 * Admin functions for Capwiz Connect integration
 */

/**
 * General settings for Capwiz Connect API usage
 */
function capwiz_connect_settings($form, $form_state) {
  $form = array();

  $form['capwiz_connect_user_batch_size'] = array(
    '#type' => 'select',
    '#options' => drupal_map_assoc(array(10, 50, 100, 200)),
    '#title' => t('User Batch Amount'),
    '#description' => t('Select the amount of Capwiz users to grab during the import process.'),
    '#default_value' => variable_get('capwiz_connect_user_batch_size', 100),
  );

  $form['capwiz_connect_action_batch_size'] = array(
    '#type' => 'select',
    '#options' => drupal_map_assoc(array(10, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000)),
    '#title' => t('Action Batch Amount'),
    '#description' => t('Select the amount of Capwiz actions to grab during the import process.'),
    '#default_value' => variable_get('capwiz_connect_action_batch_size', 100),
  );

  $form = system_settings_form($form);

  return $form;
}


/**
 * Amdin list of Capwiz Connect accounts
 */
function capwiz_connect_account_list() {
  $table_headers = array(
    'Display Name',
    array(
      'data' => 'Operations',
      'colspan' => 5,
    ),
  );

  $accounts = array();

  $results = db_query('SELECT * FROM capwiz_connect_accounts');

  if (!empty($results)) {
    foreach ($results as $record) {
      $accounts[] = array(
        $record->display_name,
        l(t('overview'), CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $record->id),
        l(t('edit'), CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $record->id . '/edit'),
        l(t('user mapping'), CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $record->id . '/user_mapping'),
        l(t('action mapping'), CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $record->id . '/action_mapping'),
        l(t('delete'), CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $record->id . '/delete'),
      );
    }
  }
  else {
    $accounts[] = array(
      array(
        'data' => t('No Capwiz Connect accounts have been added.'),
        'colspan' => count($table_headers),
      ),
    );
  }

  return theme('table', array('header' => $table_headers, 'rows' => $accounts));
}


/**
 * Form to add or edit a Capwiz Connect account
 */
function capwiz_connect_account_addoredit($form, $form_state) {
  $form = array();

  $account = isset($form_state['build_info']['args'][0]) ? $form_state['build_info']['args'][0] : NULL;

  if ($account) {
    $form['account_id'] = array(
      '#type' => 'value',
      '#value' => $account->id,
    );

    // Go to the overview page after saving
    $form['#redirect'] = CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $account->id;
  }

  $form['display_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Display Name'),
    '#description' => t('Name to show for this account. This is only used to differentiate accounts for admins.'),
    '#size' => 40,
    '#maxlength' => 255,
    '#required' => TRUE,
    '#default_value' => isset($account->display_name) ? $account->display_name : '',
  );

  $form['user_name'] = array(
    '#type' => 'textfield',
    '#title' => t('User Name'),
    '#description' => t('API username for the Capwiz Connect API.'),
    '#size' => 40,
    '#maxlength' => 255,
    '#required' => TRUE,
    '#default_value' => isset($account->user_name) ? decrypt($account->user_name, array('base64' => TRUE)) : '',
  );

  $form['user_password'] = array(
    '#type' => 'password',
    '#title' => t('User Password'),
    '#description' => t('API password for the Capwiz Connect API.'),
    '#size' => 40,
    '#maxlength' => 255,
    '#required' => !isset($account),
    '#default_value' => '',
  );

  $form['form_affiliation'] = array(
    '#type' => 'textfield',
    '#title' => t('Form Affiliation'),
    '#description' => t('Form Affiliation that can be mapped to Salesforce fields.'),
    '#size' => 40,
    '#maxlength' => 255,
    '#required' => FALSE,
    '#default_value' => isset($account->form_affiliation) ? decrypt($account->form_affiliation) : '',
  );

  // If we're editing an account display the reset sync field
  if (isset($account)) {
    $form['reset_import_date'] = array(
      '#type' => 'textfield',
      '#title' => t('Reset the import date'),
      '#description' => t('Enter a date, in any format, to reset the import settings to that date. Leave empty to not reset the current settings.'),
      '#size' => 40,
      '#maxlength' => 255,
      '#required' => FALSE,
    );
  }

  $form['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}


/**
 * Submit the add/edit form
 */
function capwiz_connect_account_addoredit_submit($form, &$form_state) {
  $values = $form_state['values'];
  $record = array(
    'display_name' => $values['display_name'],
    'user_name' => encrypt($values['user_name'], array('base64' => TRUE)),
    'form_affiliation' => $values['form_affiliation'],
  );
  if (!empty($values['user_password'])) {
    $record['user_password'] = encrypt($values['user_password'], array('base64' => TRUE));
  }

  // Determine if inserting or updating
  if (isset($values['account_id'])) {
    $record['id'] = $values['account_id'];
    // Nuke any previously-cached session information
    $session_id_cache_id = 'capwiz_session_id_' . $values['account_id'];
    $client_id_cache_id = 'capwiz_cache_id_' . $values['account_id'];
    cache_set($session_id_cache_id, FALSE, 'cache', CACHE_TEMPORARY);
    cache_set($client_id_cache_id, FALSE, 'cache', CACHE_TEMPORARY);

    // If a reset import date value was passed update those values
    if ($reset = strtotime($values['reset_import_date'])) {
      $record['user_sync_since'] = $reset;
      $record['user_sync_offset'] = 0;
      $record['action_sync_since'] = $reset;
      $record['action_sync_offset'] = 0;
    }

    $saved_record = drupal_write_record('capwiz_connect_accounts', $record, 'id');
    if ($saved_record) {
      drupal_set_message(t('Account updated'));
    }
  }
  else {
    $new_record = drupal_write_record('capwiz_connect_accounts', $record);
    if ($new_record) {
      drupal_set_message(t('New Account created'));
    }
  }
  $form_state['redirect'] = array(CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $record['id']);
}



/**
 * Show overview tab for an account
 */
function capwiz_connect_account_overview($account) {

  $build = array();
  $build['display_name'] = array(
    '#type' => 'markup',
    '#markup' => check_plain($account->display_name),
    '#prefix' => '<h2>' . t('Display Name') . '</h2>',
    '#weight' => 0,
  );

  $connection = capwiz_connect($account->id);

  $build['creds_ok'] = array(
    '#prefix' => '<h2>' .  t('Connection') . '</h2>',
    '#weight' => 1,
  );

  if ($connection->isConnected()) {
    $build['creds_ok']['#markup'] = 'Credentials OK';
  }
  else {
    $error = $connection->getLastError();
    drupal_set_message(filter_xss($error), 'error');
    $build['creds_ok']['#markup'] = t('Error connecting to Capwiz:') . ' ' . filter_xss($error);
  }

  $build['user_mapping'] = array(
    '#type' => 'markup',
    '#markup' => isset($account->mapping_user) ? 'Set' : 'Not set',
    '#prefix' => '<h2>' . t('User Mapping') . '</h2>',
    '#weight' => 2,
  );

  $build['action_mapping'] = array(
    '#type' => 'markup',
    '#markup' => (isset($account->mapping_action)) ? 'Set' : 'Not set',
    '#prefix' => '<h2>' . t('Action Mapping') . '</h2>',
    '#weight' => 3,
  );

  return $build;
}



/**
 * Confirm account delete
 */
function capwiz_connect_account_delete($form, &$form_state, $account) {
  if (isset($account)) {
    $form = array(
      '#account_id' => $account->id,
    );

    $cancel_path = CAPWIZ_CONNECT_ADMIN_URL . '/accounts/' . $account->id;

    return confirm_form(
      $form,
      t('Are you sure you want to delete the account %display_name?', array('%display_name' => $account->display_name)),
      $cancel_path,
      t('This action cannot be undone.'),
      t('Delete'),
      t('Cancel'),
      'capwiz_connect_account_delete');
  }
  else {
    drupal_set_message(t('That account does not exist.'), 'error');
    drupal_goto(CAPWIZ_CONNECT_ADMIN_URL);
  }
}


/**
 * After confirming an account delete, actually delete it
 */
function capwiz_connect_account_delete_submit($form, &$form_state) {
  $account_id = $form['#account_id'];

  // Delete the account record
  db_delete('capwiz_connect_accounts')
    ->condition('id', $account_id)
    ->execute();

  // Delete any field mappings
  $drupal = 'capwiz_connect:' . $account_id . ':%';

  db_delete('salesforce_management_field_map')
    ->condition('drupal', $drupal, 'LIKE')
    ->execute();

  drupal_set_message(t('The account has been deleted.'));
  $form_state['redirect'] = CAPWIZ_CONNECT_ADMIN_URL . '/accounts';
}





/*********************************************************/
/**
 * User mapping form functions
 */

/**
 * User field mapping for Capwiz Connect account
 * Based on sf_webform_mapping_form()
 */
function capwiz_connect_account_user_mapping($form_state, $account) {
  $base = drupal_get_path('module', 'capwiz_connect');
  drupal_add_js($base . '/capwiz_connect.js');

  $form = array(
    '#tree' => TRUE,
    '#account' => $account,
    'mapping' => array(),
    '#map_type' => 'user',
    '#theme' => 'capwiz_connect_user_mapping_form',
  );

  // Get the custom fields for users from Capwiz
  $connection = capwiz_connect($account->id);

  $user_fields['capwiz_user_id'] = (object) array(
    'label' => 'Capwiz User ID',
    'formInputType' => 'auto',
  );

  $custom_user_fields = $connection->getCustomUserFields();
  if (is_array($custom_user_fields)) {
    $user_fields += $custom_user_fields;
  }

  // Add fields for the legislator fields
  $user_fields['fed_senator_1'] = (object) array(
    'label' => 'Federal Senator 1',
    'formInputType' => 'auto',
  );
  $user_fields['fed_senator_1_phone'] = (object) array(
    'label' => 'Federal Senator 1 Phone',
    'formInputType' => 'auto',
  );
  $user_fields['fed_senator_1_email'] = (object) array(
    'label' => 'Federal Senator 1 Email',
    'formInputType' => 'auto',
  );
  $user_fields['fed_senator_2'] = (object) array(
    'label' => 'Federal Senator 2',
    'formInputType' => 'auto',
  );
  $user_fields['fed_senator_2_phone'] = (object) array(
    'label' => 'Federal Senator 2 Phone',
    'formInputType' => 'auto',
  );
  $user_fields['fed_senator_2_email'] = (object) array(
    'label' => 'Federal Senator 2 Email',
    'formInputType' => 'auto',
  );
  $user_fields['fed_rep'] = (object) array(
    'label' => 'Federal Representative',
    'formInputType' => 'auto',
  );
  $user_fields['fed_rep_phone'] = (object) array(
    'label' => 'Federal Representative Phone',
    'formInputType' => 'auto',
  );
  $user_fields['fed_rep_email'] = (object) array(
    'label' => 'Federal Representative Email',
    'formInputType' => 'auto',
  );
  $user_fields['nationalDistrict'] = (object) array(
    'label' => 'National District',
    'formInputType' => 'auto',
  );
  $user_fields['stateDistrictUpper'] = (object) array(
    'label' => 'State Upper',
    'formInputType' => 'auto',
  );
  $user_fields['stateDistrictLower'] = (object) array(
    'label' => 'State Lower',
    'formInputType' => 'auto',
  );
  $user_fields['governor'] = (object) array(
    'label' => 'Governor',
    'formInputType' => 'auto',
  );
  $user_fields['governor_phone'] = (object) array(
    'label' => 'Governor Phone',
    'formInputType' => 'auto',
  );
  $user_fields['governor_email'] = (object) array(
    'label' => 'Governor Email',
    'formInputType' => 'auto',
  );


  // Load an array of Drupal user fields for targets
  $drupal_user_fields = array(
    '' => '',
  );

  // Add profile fields to the user object if the module is enabled.
  if (module_exists('profile')) {
    // Load all the profile fields from the database.
    $result = db_query("SELECT fid, name, title, category FROM {profile_fields} ORDER BY category, weight");

    // Loop through the fields and add them to the Drupal user object.
    while ($field = db_fetch_array($result)) {
      $drupal_user_fields['Profile fields'][$field['fid']] = t('@category: @title', array('@category' => $field['category'], '@title' => $field['title']));
    }
  }
  foreach ($user_fields as $field_key => $field) {
    $map_default_value = '';
    $business_default_value = '';

    if (isset($account->mapping_user['map'][$field_key])) {
      $map_for_field = $account->mapping_user['map'][$field_key];
      $map_default_value = $account->mapping_user['map'][$field_key];
    }

    if (isset($account->mapping_user['business'][$field_key])) {
      $business_default_value = $account->mapping_user['business'][$field_key];
    }

    // Capwiz field
    $form['map']['user_field'][$field_key] = array(
      '#type' => 'item',
      '#description' => t($field->label . " - (" . $field->formInputType . ")"),
    );
    // Drupal user field
    $form['map']['drupal_user_field'][$field_key] = array(
      '#type' => 'select',
      '#options' => $drupal_user_fields,
      '#default_value' => $map_default_value,
    );
    // Overwrite business rules
    $form['map']['business'][$field_key] = array(
      '#type' => 'select',
      '#default_value' => $business_default_value,
      '#options' => array(
        '' => '',
        'blank' => t('Overwrite when blank'),
        'append' => t('Append'),
        'always' => t('Always overwrite'),
        'never' => t('Never overwrite'),
      ),
    );
  }

  // Fields specific to this account that can be mapped to Drupal user fields
  // This is a little goofy:
  //   Put an item in $form['capwiz_field_map'] for the label
  //   Put an item in $form['map']['drupal_user_field'] for the Drupal user field dropdown
  $form['capwiz_field_map']['form_affiliation'] = array(
    '#type' => 'item',
    '#description' => t('Form Affiliation'),
  );
  $form['map']['drupal_user_field']['form_affiliation'] = array(
    '#type' => 'select',
    '#options' => $drupal_user_fields,
    '#default_value' => (isset($account->mapping_user['map']['form_affiliation'])) ? $account->mapping_user['map']['form_affiliation'] : '',
  );
  // Overwrite business rules for form affiliation
  $form['map']['business']['form_affiliation'] = array(
    '#type' => 'select',
    '#default_value' => (isset($account->mapping_user['business']['form_affiliation'])) ? $account->mapping_user['business']['form_affiliation'] : '',
    '#options' => array(
      '' => '',
      'blank' => t('Overwrite when blank'),
      'append' => t('Append'),
      'always' => t('Always overwrite'),
      'never' => t('Never overwrite'),
    ),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Submit',
  );

  return $form;
}

/**
 * Handle submit of the user mapping form.
 */
function capwiz_connect_account_user_mapping_submit($form, &$form_state) {
  $form_state['values']['map']['salesforce'] = $form_state['values']['map']['drupal_user_field'];
  _capwiz_connect_save_field_mapping_save($form, $form_state, FALSE);
  drupal_set_message(t('User mapping has been saved.'));
}





/**
 * Theme the user mapping form. Based on theme_sf_webform_mapping_form()
 */
function theme_capwiz_connect_user_mapping_form($form) {
  if ($form['map']['user_field']) {
    $map = $form['map'];


    $capwiz_fields = element_children($map['user_field']);
    foreach ($capwiz_fields as $field_key) {
      $row = array();
      $row[] = array(
        'data' => drupal_render($map['user_field'][$field_key]),
        'width' => '300',
      );
      $row[] = drupal_render($map['drupal_user_field'][$field_key]);
      $row[] = drupal_render($map['business'][$field_key]);
      $rows[] = $row;
    }

    // Set the headers for the table. Action mapping does NOT include business rules
    $header = array(
      "Capwiz User Field",
      "Drupal User Field",
      "Business rules Capwiz to Drupal",
    );

    $output = theme('table', $header, $rows, array('id' => 'capwiz_connect_user_mapping_form'));

    // Additional hard-coded fields that can be mapped
    $rows = array();
    $capwiz_fields = element_children($form['capwiz_field_map']);
    foreach ($capwiz_fields as $field_key) {
      $row = array();
      $row[] = array(
        'data' => drupal_render($form['capwiz_field_map'][$field_key]),
        'width' => '300',
      );
      $row[] = drupal_render($map['drupal_user_field'][$field_key]);
      $row[] = drupal_render($map['business'][$field_key]);
      $rows[] = $row;
    }


    $header = array(
      "Capwiz Account Field",
      "Drupal User Field",
      "Business rules Capwiz to Drupal",
    );

    $output .= theme('table', $header, $rows);

    // Remove map so it's not rendered twice
    unset($form['map']);
    unset($form['capwiz_field_map']);

    $output .= drupal_render($form['submit']);
    $output .= drupal_render($form);
    return $output;
  }
}






/*********************************************************/
/**
 * Action mapping form functions
 */


/**
 * Action field mapping for Capwiz Connect account
 * Similar to capwiz_connect_account_user_mapping() - make sure that any structural
 * changes made here are reflected there
 */
function capwiz_connect_account_action_mapping($form_state, $account) {
  $base = drupal_get_path('module', 'capwiz_connect');
  drupal_add_js($base . '/capwiz_connect.js');

  $form = array(
    '#tree' => TRUE,
    '#account' => $account,
    'mapping' => array(),
    '#map_type' => 'action',
    '#theme' => 'capwiz_connect_account_mapping_form',
  );

  // only show object selection for webforms that haven't been mapped
  if (empty($account->mapping_action)) {
    $form['object'] = array(
      '#type' => 'select',
      '#options' => _get_salesforce_objects(),
      '#default_value' => $account->mapping_action['sf_object'],
      '#title' => t('Salesforce Object'),
      '#description' => t('Choose the Salesforce object you want to create for each Capwiz action.'),
      '#ajax' => array(
        'callback' => 'js/capwiz_connect/salesforce-object-fields',
        'wrapper' => 'map',
        'effect' => 'fade',
      ),
    );
  }
  else {
    $form['object'] = array(
      '#prefix' => t("<div>Actions from this Capwiz account are currently mapped to the <strong>%sf_object</strong> object.</div>", array('%sf_object' => $account->mapping_action['sf_object'])),
      '#type' => 'hidden',
      '#default_value' => $account->mapping_action['sf_object'],
      '#suffix' => t('<div>Use the <strong>Remove Map</strong> button below if you wish to map to a different Salesforce object.</div>'),
    );
  }

  $form['map'] = array(
    '#prefix' => '<div id="map">',
    '#suffix' => '</div>',
    '#value' => '&nbsp;',
  );

  // Only build the field mapping form if a Salesforce object type has been
  // selected to map to
  if ($form_state['values']['object'] || !empty($account->mapping_action['sf_object'])) {
    $object = !empty($account->mapping_action['sf_object']) ? $account->mapping_action['sf_object'] : $form_state['values']['object'];

    // Get the fields available on this type of Salesforce object
    $object_details = _capwiz_connect_get_sf_object_details($object);
    if (!empty($object_details)) {
      $fields = $object_details['fields'];
      $record_types = $object_details['record_types'];

      // Create a drop down list friendly version of the Salesforce fields
      foreach ($fields as $key => $value) {
        $options[$key] = $value['label'];
      }

      $disabled = !empty($account->mapping_action['sf_object']) ? FALSE : TRUE;

      $form['record_type'] = array(
        '#type' => 'select',
        '#title' => t('Record Type'),
        '#description' => t('The type of !name to create', array('!name' => $account->mapping_action['sf_object'])),
        '#options' => $record_types,
        '#default_value' => $account->mapping_action['recordtype_id'],
      );

      // Define the Capwiz fiels that can be defined to SF fields
      // Field name as key, displayable label as the value
      $capwiz_fields = array(
        'alertId' => 'Alert ID',
        'alertTitle' => 'Alert Title',
        'action_timestamp' => 'Action Timestamp',
        'emailAddress' => 'Email Address',
        'messageId' => 'Message ID',
        'userId' => 'Capwiz User ID',
      );

      // Only allow mapping to the Contact in SF if the SF User Integration module is available
      if (module_exists('sf_user')) {
        $capwiz_fields['sf_userid'] = 'Salesforce User Record';
      }

      foreach ($capwiz_fields as $field_key => $field_label) {
        $map_for_field = $account->mapping_action['map'][$field_key];

        // Capwiz field
        $form['map']['user_field'][$field_key] = array(
          '#type' => 'item',
          '#description' => t($field_label),
        );
        // Salesforce field
        $form['map']['salesforce'][$field_key] = array(
          '#type' => 'select',
          '#options' => $options,
          '#default_value' => $account->mapping_action['map'][$field_key],
        );

        if ($map_for_field && ($fields[$map_for_field]['idLookup'] || $fields[$map_for_field]['externalId'])) {
          if ($account->mapping_action['dedupe_key'] == $map_for_field) {
            $value = '<a class="dedupe-link" href="/js/capwiz_connect/' . $account->id . '/remove-dedupe-field/action">Remove dedupe field</a>';
          }
          else {
            $value = '<a class="dedupe-link" href="/js/capwiz_connect/' . $account->id . '/set-dedupe-field/action/' . $map_for_field . '">Set as dedupe field</a>';
          }
        }
        else {
          $value = '';
        }
        $form['map']['dedupe'][$field_key] = array(
          '#value' => $value,
        );
      }

      // Fields specific to this account that can be mapped to SF
      // This is a little goofy:
      //   Put an item in $form['capwiz_field_map'] for the label
      //   Put an item in $form['map']['salesforce'] for the SF field dropdown
      $form['capwiz_field_map']['form_affiliation'] = array(
        '#type' => 'item',
        '#description' => t('Form Affiliation'),
      );
      $form['map']['salesforce']['form_affiliation'] = array(
        '#type' => 'select',
        '#options' => $options,
        '#default_value' => $account->mapping_action['map']['form_affiliation'],
      );
    }

    $form['remove'] = array(
      '#type' => 'submit',
      '#value' => t('Remove Map'),
      '#submit' => array('capwiz_connect_remove_mapping'),
    );

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => 'Submit',
    );
  }
  return $form;
}

/**
 * Handle submit of the action mapping form. Just calls _capwiz_connect_save_field_mapping_save()
 */
function capwiz_connect_account_action_mapping_submit($form, &$form_state) {
  return _capwiz_connect_save_field_mapping_save($form, $form_state);
}





/*********************************************************/
/**
 * Generic functions used for both user and action mapping
 */


/**
 * Submit either mapping form. Called by capwiz_connect_account_user_mapping_submit() and
 * capwiz_connect_account_action_mapping_submit()
 */
function _capwiz_connect_save_field_mapping_save($form, &$form_state, $show_saved_message = TRUE) {
  $map_type = $form['#map_type'];
  if ($form_state['values']['map']['salesforce']) {
    $drupal_fieldmap_name = 'capwiz_connect:' . $form['#account']->id . ':' . $map_type;
    $fieldmap_id = db_result(db_query("SELECT fieldmap FROM {salesforce_management_field_map} WHERE drupal = '%s'", $drupal_fieldmap_name));
    $record_type = $form_state['values']['record_type'];

    // Display name for the field map
    $name = $form['#account']->display_name . " (" . ucfirst($map_type) . " field mapping)";

    // Prep array for saving to field map table
    $map = array(
      'fieldmap' => $fieldmap_id,
      'name' => $name,
      'drupal' => $drupal_fieldmap_name,
      'salesforce' => $form_state['values']['object'],
      'recordtype_id' => $record_type,
    );

    // Setup field mapping
    $fields = array();
    foreach ($form_state['values']['map']['salesforce'] as $key => $value) {
      if (!empty($value)) {
        $fields[$key] = $value;
      }
    }
    $map['fields'] = serialize($fields);

    // Build array of business rules (overwrite when blank, etc)
    $business = array();
    if (is_array($form_state['values']['map']['business'])) {
      foreach ($form_state['values']['map']['business'] as $key => $value) {
        if (!empty($value)) {
          $business[$key] = $value;
        }
      }
    }
    $map['business'] = serialize($business);


    salesforce_management_api_fieldmap_save($map);

    if ($show_saved_message) {
      drupal_set_message(t('Salesforce mapping has been saved. Please select a dedupe field if you want to prevent duplicate objects from being created in Salesforce.'));
    }
  }
}



/**
 * Theme the mapping form. Based on theme_sf_webform_mapping_form()
 * Changed made here need to be reflected in
 * capwiz_connect_render_salesforce_object_fields(), since both render the form
 * in different situations.
 */
function theme_capwiz_connect_account_mapping_form($form) {
  if ($form['map']['user_field']) {
    $output = drupal_render($form['object']);
    $output .= drupal_render($form['record_type']);
    $map = $form['map'];

    $capwiz_fields = element_children($map['user_field']);
    foreach ($capwiz_fields as $field_key) {
      $row = array();
      $row[] = array(
        'data' => drupal_render($map['user_field'][$field_key]),
        'width' => '300',
      );
      $row[] = drupal_render($map['salesforce'][$field_key]);
      if ($form['#map_type'] != 'action') {
        $row[] = drupal_render($map['business'][$field_key]);
      }
      $row[] = drupal_render($map['dedupe'][$field_key]);
      $rows[] = $row;
    }

    // Set the headers for the table. Action mapping does NOT include business rules
    if ($form['#map_type'] == 'action') {
      $header = array(
        "Capwiz Field",
        "Salesforce Field",
        "Dedupe Field",
      );
    }
    else {
      $header = array(
        "Capwiz Field",
        "Salesforce Field",
        "Business rules Drupal to SF",
        "Dedupe Field",
      );
    }

    $output .= theme('table', $header, $rows, array('id' => 'capwiz_connect_account_mapping_form'));

    // Additional hard-coded fields that can be mapped
    $rows = array();
    $capwiz_fields = element_children($form['capwiz_field_map']);
    foreach ($capwiz_fields as $field_key) {
      $row = array();
      $row[] = array(
        'data' => drupal_render($form['capwiz_field_map'][$field_key]),
        'width' => '300',
      );
      $row[] = drupal_render($map['salesforce'][$field_key]);
      if ($form['#map_type'] != 'action') {
        $row[] = drupal_render($map['business'][$field_key]);
      }
      $rows[] = $row;
    }


    // Set the headers for the table. Action mapping does NOT include business rules
    if ($form['#map_type'] == 'action') {
      $header = array(
        "Capwiz Account Field",
        "Salesforce Field",
      );
    }
    else {
      $header = array(
        "Capwiz Account Field",
        "Salesforce Field",
        "Business rules Drupal to SF",
      );
    }

    $output .= theme('table', $header, $rows);

    // Remove map so it's not rendered twice
    unset($form['map']);
    unset($form['capwiz_field_map']);

    $output .= drupal_render($form['submit']);
    $output .= drupal_render($form['remove']);
    $output .= drupal_render($form);
    return $output;
  }
}

/**
 * AHAH callback for mapping form
 * Copy of render_salesforce_object_fields() from sf_webform module
 */
function capwiz_connect_render_salesforce_object_fields($form, &$form_state) {
  $output = drupal_render($form['record_type']);

  $map = $form['map'];

  // Add all the mappable fields as elements in an array to be passed into the
  // theme function
  $rows = array();
  $capwiz_fields = element_children($map['user_field']);
  foreach ($capwiz_fields as $field_key) {
    $row = array();
    $row[] = array(
      'data' => drupal_render($map['user_field'][$field_key]),
      'width' => '300',
    );
    $row[] = drupal_render($map['salesforce'][$field_key]);
    if ($form['#map_type'] != 'action') {
      $row[] = drupal_render($map['business'][$field_key]);
    }
    $row[] = drupal_render($map['dedupe'][$field_key]);
    $rows[] = $row;
  }

  // Set the headers for the table. Action mapping does NOT include business rules
  if ($form['#map_type'] == 'action') {
    $header = array(
      "Capwiz Field",
      "Salesforce Field",
      "Dedupe Field",
    );
  }
  else {
    $header = array(
      "Capwiz Field",
      "Salesforce Field",
      "Business rules Drupal to SF",
      "Dedupe Field",
    );
  }

  $output .= theme('table', $header, $rows, array('id' => 'capwiz_connect_account_mapping_form'));

  // Additional hard-coded fields that can be mapped
  $rows = array();
  $capwiz_fields = element_children($form['capwiz_field_map']);
  foreach ($capwiz_fields as $field_key) {
    $row = array();
    $row[] = array(
      'data' => drupal_render($form['capwiz_field_map'][$field_key]),
      'width' => '300',
    );
    $row[] = drupal_render($map['salesforce'][$field_key]);
    if ($form['#map_type'] != 'action') {
      $row[] = drupal_render($map['business'][$field_key]);
    }
    $rows[] = $row;
  }

  // Set the headers for the table. Action mapping does NOT include business rules
  if ($form['#map_type'] == 'action') {
    $header = array(
      "Capwiz Account Field",
      "Salesforce Field",
    );
  }
  else {
    $header = array(
      "Capwiz Account Field",
      "Salesforce Field",
      "Business rules Drupal to SF",
    );
  }

  $output .= theme('table', $header, $rows);

  $output .= drupal_render($form['submit']);

  return $output;
}


/**
 * Submit handler to delete the salesforce map from a user
 */
function capwiz_connect_remove_mapping($form, &$form_state) {
  $drupal = 'capwiz_connect:' . $form['#account']->id . ':' . $form['#map_type'];
  db_query("DELETE FROM {salesforce_management_field_map} WHERE drupal = '%s'", $drupal);
  drupal_set_message(t('Salesforce mapping has been removed'));
}


/**
 * Callback for AJAX function to set the dedupe field on a mapping
 *
 * @param $acount
 *   Account object
 * @param $map_type
 *   String of mapping type: user or action
 * @param $field_name
 *   Name of the field to use as the dedupe
 */
function capwiz_connect_add_dedupe_field($account, $map_type, $field_name) {
  if ($account && $map_type && $field_name) {
    $drupal = 'capwiz_connect:' . $account->id . ':' . $map_type;
    db_query("UPDATE {salesforce_management_field_map} SET dedupe_key = '%s' WHERE drupal = '%s'", $field_name, $drupal);
  }
}

/**
 * Callback for AJAX function to remove the dedupe field on a mapping
 *
 * @param $acount
 *   Account object
 * @param $map_type
 *   String of mapping type: user or action
 */
function capwiz_connect_remove_dedupe_field($account, $map_type) {
  if ($account && $map_type) {
    $drupal = 'capwiz_connect:' . $account->id . ':' . $map_type;
    db_query("UPDATE {salesforce_management_field_map} SET dedupe_key = NULL WHERE drupal = '%s'", $drupal);
  }
}






/**
 * Function to return an array of object fields
 * Copy of _get_object_details() from sf_webform module
 */
function _capwiz_connect_get_sf_object_details($object_name) {
  $sf = salesforce_management_api_connect();
  if ($sf) {
    $record_types = array();
    try {
      $object = $sf->client->describeSObject($object_name);
    }
    catch (SoapFault $e) {
      // Something failed in getting object details from Salesforce
      drupal_set_message($e->getMessage(), 'error');
      if ($e->faultcode == 'sf:INVALID_TYPE') {
        drupal_set_message(t('The Salesforce object type selected for this mapping is no longer available. Either re-add that object type in Salesforce, or click Remove Map at the bottom of the page and re-do the mapping with a difference object type.'), 'error');
      }
      return array();
    }
    catch (Exception $e) {
      // Something failed in getting object details from Salesforce
      drupal_set_message($e->getMessage());
      return array();
    }
    // get object fields
    foreach ($object->fields as $field) {
      $object_fields[$field->name] = array(
        'label' => $field->label,
        'idLookup' => $field->idLookup,
        'externalId' => $field->externalId,
      );
    }

    // get object record types
    foreach ($object->recordTypeInfos as $record_type) {
      if ($record_type->available && $record_type->name != 'Master') {
        $record_types[$record_type->recordTypeId] = $record_type->name;
      }
    }

    $object_fields[] = '--Select One--';
    $record_types[] = '--Select One--';

    asort($object_fields);
    asort($record_types);

    return array(
      'fields' => $object_fields,
      'record_types' => $record_types,
    );
  }
  return array();
}
