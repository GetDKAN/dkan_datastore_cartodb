<?php

/**
 * @file
 * CartoDB Datastore class.
 */
class CartoDbDatastore extends Datastore implements DatastoreForm {

  public $endpoint = '.cartodb.com/api/v1/imports';

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
    // CartoDB Settings.
    $this->table_id = '';
    $this->table_name = '';
    $this->queue_id = '';
    $result = db_query("SELECT queue_id, table_id, table_name FROM {dkan_datastore_cartodb} WHERE uuid = :uuid", array(':uuid' => $this->uuid));
    $this->cartoStatus = self::CARTODB_STATUS_EMPTY;
    foreach ($result as $record) {
      if ($record->table_id) {
        $this->table_id = $record->table_id;
        $this->table_name = $record->table_name;
        $this->cartoStatus = self::CARTODB_STATUS_IMPORTED;
      }
      elseif ($record->queue_id) {
        $this->queue_id = $record->queue_id;
        $this->cartoStatus = self::CARTODB_STATUS_UPLOADING;
      }
    }
  }

  function sync() {
    $url = $this->endpoint . '/' . $this->queue_id . '?api_key=' . $this->key ;
    $request = drupal_http_request($url);
    $data = drupal_json_decode($request->data);
    $record = new stdClass();
    $record->uuid = $this->uuid;
    $record->queue_id = $data['queue_id'];
    $this->queue_id = $record->queue_id;
    if ($data['state'] == 'complete') {
      $record->table_id = $data['table_id'];
      $record->table_name = $data['table_name'];
      $this->table_id = $record->table_id;
      $this->table_name = $record->table_name;
    }
    drupal_write_record('dkan_datastore_cartodb', $record);
  }

  function status() {
    if ($this->cartoStatus == self::CARTODB_STATUS_IMPORTED) {
      // TODO: Make a theme function.
      return t('File has been uploaded to CartoDB.') . '<p> <strong>CartoDB Table ID:</strong> ' . $this->table_id . '<br><strong>CartoDB Table Name:</strong> ' . $this->table_name;
    }
    elseif ($this->cartoStatus == self::CARTODB_STATUS_UPLOADING) {
      return t('File uploading');
    }
    else {
      return t('File not uploaded');
    }
    return FALSE;
  }

  public function apiUri() {
    return FALSE;
  }

  public function rows() {
    return FALSE;
  }

  public function headers() {
    return FALSE;
  }

  public function import() {
    $this->fileUrl = $this->fileUrl();
    $request = drupal_http_request($this->endpoint, array('api_key' => $this->key));
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
    $request = drupal_http_request($this->endpoint, $options);
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
      drupal_set_message("Request failed - ". $request->error .' - '. http_build_query($params));
    }

  }

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
    $form['source']['status'] = array(
      '#title' => t('CartoDB Datastore Status'),
      '#type' => 'item',
      '#markup' => '<br>' . $this->status(),
    );
    $form = confirm_form($form, t('Import all content from source?'), 'node/' . $this->node->nid, '', t('Import'), t('Cancel'), 'confirm feeds update');
    return $form;

  }

  public function manageFormSubmit(&$form_state) {
    $this->import();
  }

  public function deleteForm(&$form_state) {
    $node = $this->node;

    $form = array();
    $form['#redirect'] = 'node/' . $node->nid;

    return $form;
  }

  public function deleteFormSubmit(&$form_state) {}

  public function dropForm(&$form_state) {
    $node = $this->node;
    $form = array();
    $form['#redirect'] = 'node/' . $node->nid;
    $form = confirm_form($form, t('Drop this datastore?'), $form['#redirect'], '', t('Drop'), t('Cancel'), 'confirm drop');

  }

  public function dropFormSubmit(&$form_state) {}

  public function apiForm() {
    // TODO: Make a theme function.
    $output = '<h2>' . t('CartoDB Datastore API') . '</h2>';
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
  $output .="--$boundary--";
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

