<?php
/**
 * @file
 * Utility functions.
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
  $upload_size = min((int) ini_get('post_max_size'), (int) ini_get('upload_max_filesize'));
  $extensions = array('xml');
  return array(
    'file' => array(
      '#title' => t('Document'),
      '#type' => 'managed_file',
      '#required' => TRUE,
      '#description' => filter_xss(t('Select document to upload.<br/>Files must be less than <b>!size MB.</b><br/>Allowed file types: <b>!ext.</b>', array('!size' => $upload_size, '!ext' => $extensions[0])), array('br', 'b')),
      '#default_value' => isset($form_state['values']['files']) ? $form_state['values']['files'] : NULL,
      '#upload_location' => 'temporary://',
      '#upload_validators' => array(
        'file_validate_extensions' => $extensions,
         // Assume its specified in MB.
        'file_validate_size' => array($upload_size * 1024 * 1024),
      ),
    ),
  );
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
  $object = islandora_ingest_form_get_object($form_state);
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
