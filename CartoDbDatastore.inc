<?php

/**
 * @file
 * CartoDB Datastore class.
 */
class CartoDbDatastore extends Datastore implements DatastoreForm {

  public $endpoint = '.cartodb.com/api/v1/imports';

  /**
   * Constructor, set id and load default configuration.
   */
  protected function __construct($uuid) {
    $this->uuid = $uuid;
    $this->node = $this->node($uuid);
    $this->cartoUser = variable_get('dkan_datastore_cartodb_user', '');
    $this->key = variable_get('dkan_datastore_cartodb_key', '');
    $this->endpoint = "https://" . $this->cartoUser . $this->endpoint;
  }

  public function status() {
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
        drupal_set_message("Request suceeded");
        $options = array(
          'data' => 'api_key=' . $this->key,
          'method' => 'POST',
        );
        $url = $this->endpoint . '/' . $data['item_queue_id'];
        $request = drupal_http_request($url, $options);
      }
    }
    elseif (isset($request->error)) {
      drupal_set_message("Request failed - ". $request->error .' - '. http_build_query($params));
    }

  }

  public function manageForm(&$form_state) {
    $form = array();
    $form['source_status'] = array(
      '#type' => 'fieldset',
      '#title' => t('CartoDB Datastore Status'),
      '#tree' => TRUE,
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

