<?php

/**
 * @file
 * CartoDB Datastore class.
 */

class CartoDbDatastore extends Datastore implements DatastoreForm {

  public $endpoint = '.cartodb.com/api/v1';

  const CARTODB_STATUS_EMPTY = 0;
  const CARTODB_STATUS_UPLOADING = 1;
  const CARTODB_STATUS_IMPORTED = 2;

  /**
   * Constructor, set id and load default configuration.
   */
  protected function __construct($uuid) {
    $this->uuid = $uuid;
    $this->node = $this->node($uuid);
    $this->cartoUser = variable_get('dkan_datastore_cartodb_user', '');
    $this->key = variable_get('dkan_datastore_cartodb_key', '');
    $this->endpoint = "https://" . $this->cartoUser . $this->endpoint;
    $this->new = TRUE;
    // CartoDB settings.
    $this->table_id = '';
    $this->table_name = '';
    $this->queue_id = '';
    $this->viz_id = '';
    $this->cartoStatus = self::CARTODB_STATUS_EMPTY;
    // Initialize status.
    $result = db_query("SELECT viz_id, queue_id, table_id, table_name FROM {dkan_datastore_cartodb} WHERE uuid = :uuid", array(':uuid' => $this->uuid));
    foreach ($result as $record) {
      if ($record->table_id) {
        $this->table_id = $record->table_id;
        $this->table_name = $record->table_name;
        $this->viz_id = $record->viz_id;
        $this->cartoStatus = self::CARTODB_STATUS_IMPORTED;
        $this->new = FALSE;
      }
      elseif ($record->queue_id) {
        $this->queue_id = $record->queue_id;
        $this->cartoStatus = self::CARTODB_STATUS_UPLOADING;
        $this->new = FALSE;
      }
    }
  }

  /**
   * Updates or inserts record.
   */
  public function save() {
    $record = new stdClass();
    $record->uuid = $this->uuid;
    $record->queue_id = $this->queue_id;
    $record->table_id = $this->table_id;
    $record->viz_id = $this->viz_id;
    $record->table_name = $this->table_name;
    if ($this->new) {
      drupal_write_record('dkan_datastore_cartodb', $record);
    }
    else {
      drupal_write_record('dkan_datastore_cartodb', $record, 'uuid');
    }
    $this->new = FALSE;
  }

  /**
   * Checks status of queued table.
   */
  public function sync() {
    $url = $this->endpoint . '/imports/' . $this->queue_id . '?api_key=' . $this->key;
    $request = drupal_http_request($url);
    $data = drupal_json_decode($request->data);
    $this->queue_id = $data['queue_id'];
    if ($data['state'] == 'complete') {
      $this->table_id = $data['table_id'];
      $this->table_name = $data['table_name'];
    }
    $this->save();
  }

  /**
   * Gets a list of available visualizations.
   */
  private function getVizOptions() {
    $url = $this->endpoint . '/viz?api_key=' . $this->key;
    $request = drupal_http_request($url);
    $options = array();
    if ($request->code == 200) {
      $data = json_decode($request->data);
      if (isset($data->visualizations)) {
        foreach ($data->visualizations as $viz) {
          if ($viz->type == 'derived') {
            $options[$viz->id] = $viz->name;
          }
        }
      }
    }
    return $options;
  }

  /**
   * Gets public API url.
   */
  public function apiUri() {
    return FALSE;
  }

  /**
   * Imports file from DKAN to table in CartoDB.
   */
  public function import() {
    $this->fileUrl = $this->fileUrl();
    $boundary = 'A0sFSD';
    $headers = array("Content-Type" => "multipart/form-data; boundary=$boundary");
    $params = array('api_key' => $this->key, 'file' => $this->fileUrl);
    $data = dkan_datastore_cartodb_multipart_encode($boundary, $params);
    $options = array(
      'method' => 'POST',
      'data' => $data,
      'timeout' => 15,
      'headers' => $headers,
    );
    $request = drupal_http_request($this->endpoint . '/imports', $options);
    if (isset($request->data) && $data = drupal_json_decode($request->data)) {
      if ($data['success']) {
        // TODO: keep checking until uploaded.
        drupal_set_message("Request suceeded");
        $this->cartoStatus = self::CARTODB_STATUS_UPLOADING;
        $this->queue_id = $data['item_queue_id'];
        $this->sync();
      }
    }
    elseif (isset($request->error)) {
      drupal_set_message("Request failed - " . $request->error . ' - ' . http_build_query($params));
    }

  }

  /**
   * Allows user to import file or choose visualization.
   */
  public function manageForm(&$form_state) {
    // Latest status was not complete.
    if ($this->cartoStatus == self::CARTODB_STATUS_UPLOADING) {
      $this->sync();
    }
    $this->status();
    $form = array();
    $form['source'] = array(
      '#type' => 'fieldset',
      '#title' => t('CartoDB Datastore Status'),
      '#tree' => TRUE,
    );
    if ($this->cartoStatus == self::CARTODB_STATUS_IMPORTED) {
      $table_link = 'http://' . $this->cartoUser . '.cartodb.com/tables/' . $this->table_name;
      $form['source']['status'] = array(
        '#type' => 'item',
        '#markup' => t('Your resource has been uploaded as a table in CartoDB. You can view or edit the table directly here: !here.', array('!here' => l($table_link, $table_link))),
      );
      $options = $this->getVizOptions();
      if ($options) {
        $form['source']['viz'] = array(
          '#type' => 'select',
          '#title' => t('Visualizations.'),
          '#description' => t('Select a visualization above to preview on the resource page. If none are selected resource will be previewed by Recline settings.'),
          '#default_value' => $this->viz_id,
          '#options' => array('' => t('--  none --')) + $options,
        );
        $form['source']['submit'] = array(
          '#type' => 'submit',
          '#value' => t('Select Visualization'),
        );
      }
    }
    elseif ($this->cartoStatus == self::CARTODB_STATUS_UPLOADING) {
      $form['source']['status'] = array(
        '#title' => t('File Uploading...'),
        '#type' => 'item',
        '#prefix' => '<p></p><span style="float: left; padding: 0px 10px 20px 0;" class="icon glyphicon glyphicon-refresh" aria-hidden="true"></span>',
      );
    }
    else {
      $form['source']['status'] = array(
        '#title' => t('File not yet imported. Click "import" to push your file to CartoDB. The file will exist as "Table" in CartodDB. You will be able to edit the table or create a visualization from the table in CartoDB.'),
        '#type' => 'item',
      );
      $form = confirm_form($form, t('Import all content from source?'), 'node/' . $this->node->nid, '', t('Import'), t('Cancel'), 'confirm feeds update');
    }
    return $form;

  }

  /**
   * Manage submit function.
   */
  public function manageFormSubmit(&$form_state) {
    if ($form_state['triggering_element']['#id'] == 'edit-source-submit') {
      $this->viz_id = $form_state['values']['source']['viz'];
      $this->save();
      drupal_set_message(t('Visualization has been updated.'));
      drupal_goto('node/' . $this->node->nid);
    }
    else {
      $this->import();
    }
  }

  /**
   * Deletes items from table in CartoDB.
   *
   * TODO: Implement.
   */
  public function deleteForm(&$form_state) {
    $node = $this->node;

    $form = array();
    $form['#redirect'] = 'node/' . $node->nid;

    return $form;
  }

  /**
   * Delete submit function.
   *
   * TODO: Implement.
   */
  public function deleteFormSubmit(&$form_state) {}

  /**
   * Deletes table from CartoDB.
   */
  public function dropForm(&$form_state) {
    $node = $this->node;
    $form = array();
    $form['#redirect'] = 'node/' . $node->nid;
    $form = confirm_form($form, t('Drop this datastore?'), $form['#redirect'], '', t('Drop'), t('Cancel'), 'confirm drop');

  }

  /**
   * Delete submit function.
   *
   * TODO: Implement.
   */
  public function dropFormSubmit(&$form_state) {}

  /**
   * Provides link and instructions to API.
   */
  public function apiForm() {
    // TODO: Make a theme function.
    $output = '<h2>' . t('CartoDB Datastore API') . '</h2>';
    if ($this->table_id) {
      $output .= t('Access resource data through the CartoDB API.');
      $output .= '<h3>' . t('Table Name') . '</h3>';
      $output .= $this->table_name;
      $output .= '<h3>' . t('Example Query') . '</h3>';
      $url = 'http://' . $this->cartoUser . '.cartodb.com/api/v2/sql?q=SELECT count(*) FROM ' . $this->table_name;
      $output .= l($url, $url);
      $output .= '<h3>' . t('Documentation') . '</h3>';
      $output .= l(t('CartoDB SQL API Documentation'), 'http://docs.cartodb.com/cartodb-platform/sql-api.html');
    }
    else {
      $output .= t('Resource not yet imported into CartoDB');
    }
    return $output;
  }

}

/**
 * Helps encode for file submission.
 */
function dkan_datastore_cartodb_multipart_encode($boundary, $params) {
  $output = "";
  foreach ($params as $key => $value) {
    $output .= "--$boundary\r\n";
    if ($key == 'file') {
      $output .= dkan_datastore_cartodb_multipart_enc_file($value);
    }
    else {
      $output .= dkan_datastore_carto_multipart_enc_text($key, $value);
    }
  }
  $output .= "--$boundary--";
  return $output;
}

/**
 * Adds header information for name/value pair.
 */
function dkan_datastore_carto_multipart_enc_text($name, $value) {
  return "Content-Disposition: form-data; name=\"$name\"\r\n\r\n$value\r\n";
}

/**
 * Loads file for the big push.
 */
function dkan_datastore_cartodb_multipart_enc_file($path) {
  if (substr($path, 0, 1) == "@") {
    $path = substr($path, 1);
  }
  $filename = basename($path);
  $mimetype = "application/octet-stream";
  $data = "Content-Disposition: form-data; name=\"file\"; filename=\"$filename\"\r\n";
  $data .= "Content-Transfer-Encoding: binary\r\n";
  $data .= "Content-Type: $mimetype\r\n\r\n";
  $data .= file_get_contents($path) . "\r\n";
  return $data;
}
