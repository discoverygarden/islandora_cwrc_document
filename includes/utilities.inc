<?php
/**
 * @file
 * Utility functions.
 */

/**
 * Helper to load up the CWRC Writer.
 *
 * @param array $form
 *   An array representing a form within Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 * @param AbstractDatastream $datastream
 *   An AbstractDatastream representing a datastream on an object within Fedora.
 *
 * @return array
 *   An array to be rendered.
 */
function islandora_cwrc_document_display_cwrc_writer($form, $form_state, AbstractDatastream $datastream) {
  module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');
  $object = $datastream->parent;
  $form['cwrc_writer'] = array(
    '#theme' => 'islandora_cwrc_writer',
    '#object' => $object,
    '#schema' => islandora_cwrc_writer_get_document_schema_pid($object),
    '#documents' => array($object->id),
    '#sources' => array(),
  );
  return $form;
}
