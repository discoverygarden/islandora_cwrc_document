<?php
/**
 * @file
 * Upload a CWRC-Document's Objects CWRC datastream.
 */

/**
 * Defines the upload form for documents for CWRC.
 *
 * @param array $form
 *   An array representing a form within Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 *
 * @return array
 *   An array representing a Drupal form to be rendered.
 */
function islandora_cwrc_document_upload_form($form, $form_state) {
  module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');
  $upload_size = min((int) ini_get('post_max_size'), (int) ini_get('upload_max_filesize'));
  $extensions = array('xml');
  $schemas = islandora_cwrc_writer_get_schemas();
  $schema_options = array();
  foreach ($schemas as $schema) {
    $pid = $schema['pid'];
    $name = $schema['name'];
    $schema_options[$pid] = $name;
  }
  return array(
    'schema' => array(
      '#title' => t('Schema'),
      '#description' => t('Please select a valid schema for this document.'),
      '#type' => 'select',
      '#required' => TRUE,
      '#options' => $schema_options,
    ),
    'file' => array(
      '#title' => t('Document'),
      '#type' => 'managed_file',
      '#required' => TRUE,
      '#description' => t('Select file to upload.<br/>Files must be less than <strong>@size MB.</strong><br/>Allowed file types: <b>@ext.</b>', array('@size' => $upload_size, '@ext' => $extensions[0])),
      '#default_value' => isset($form_state['values']['file']) ? $form_state['values']['file'] : NULL,
      '#upload_location' => file_default_scheme() . '://',
      '#upload_validators' => array(
        'file_validate_extensions' => $extensions,
         // Assume its specified in MB.
        'file_validate_size' => array($upload_size * 1024 * 1024),
      ),
    ),
  );
}

/**
 * Checks that the given document is validate according to the given schema.
 */
function islandora_cwrc_document_upload_form_validate(array $form, array &$form_state) {
  global $user;
  $file = file_load($form_state['values']['file']);
  // The #required field will handle the validation in this case.
  if ($file === FALSE) {
    return;
  }
  $schema_pid = $form_state['values']['schema'];
  $data = array(
    'sch' => url("islandora/object/{$schema_pid}/datastream/SCHEMA/view", array('absolute' => TRUE)),
    'type' => 'RNG_XML',
    'content' => file_get_contents($file->uri),
  );
  $data_string = drupal_json_encode($data);
  $handle = curl_init();
  curl_setopt($handle, CURLOPT_URL, url('cwrc/services/validator/validate.html', array(
        'absolute' => TRUE,
      )));
  curl_setopt($handle, CURLOPT_HTTPHEADER, array(
      'Accept: application/xml, text/xml, */*',
      'Content-Type: application/x-www-form-urlencoded',
    ));
  curl_setopt($handle, CURLOPT_USERPWD, "{$user->name}:{$user->pass}");
  curl_setopt($handle, CURLOPT_POST, TRUE);
  curl_setopt($handle, CURLOPT_POSTFIELDS, http_build_query($data));
  curl_setopt($handle, CURLOPT_FOLLOWLOCATION, TRUE);
  curl_setopt($handle, CURLOPT_RETURNTRANSFER, TRUE);
  $result = curl_exec($handle);
  $status = curl_getinfo($handle, CURLINFO_HTTP_CODE);
  switch ($status) {
    case 200:
      $xml = simplexml_load_string($result);
      $validation_result = (string) $xml->status;
      $errors = array();
      if ($validation_result !== 'pass') {
        $errors[] = t('The document is not valid, please uploaded a valid document for the selected schema.');
      }
      foreach ($xml->warning as $warning) {
        $errors[] = t('Path: %path</br>Message: %message', array(
                     '%path' => (string) $warning->path,
                     '%message' => (string) $warning->message));
      }
      if (!empty($errors)) {
        form_set_error('file', implode('</br>', $errors));
      }
      break;

    default:
      form_set_error('file', t('Failed to validate uploaded document: Status Code: %status.', array(
            '%status' => $status)));
      break;
  }
  curl_close($handle);
}

/**
 * Submit handler, adds uploaded file to the object.
 *
 * @param array $form
 *   An array representing a form within Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 */
function islandora_cwrc_document_upload_form_submit($form, $form_state) {
  module_load_include('inc', 'islandora', 'includes/ingest.form.inc');
  module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');
  $object = islandora_ingest_form_get_object($form_state);
  $schema = islandora_object_load($form_state['values']['schema']);
  islandora_cwrc_writer_set_document_schema($object, $schema);
  if (!isset($object['CWRC'])) {
    $ds = $object->constructDatastream('CWRC', 'M');
    $object->ingestDatastream($ds);
  }
  else {
    $ds = $object['CWRC'];
  }
  $file = file_load($form_state['values']['file']);
  $ds->setContentFromFile($file->uri, FALSE);
  $ds->label = $file->filename;
  $ds->mimetype = 'application/xml';
}
