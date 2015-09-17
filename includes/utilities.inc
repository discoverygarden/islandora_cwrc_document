<?php
/**
 * @file
 * Utility functions.
 */

/**
 * Helper to load up the CWRC Writer.
 *
 * @param AbstractObject $object
 *   An object to be edited.
 *
 * @return array
 *   An array to be rendered.
 */
function islandora_cwrc_document_display_cwrc_writer(AbstractObject $object) {
  module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/breadcrumb');
  drupal_set_title($object->label);
  drupal_set_breadcrumb(islandora_get_breadcrumbs($object));
  return array(
    'cwrc_writer' => array(
      '#theme' => 'islandora_cwrc_writer',
      '#object' => $object,
      '#schema' => islandora_cwrc_writer_get_document_schema_pid($object),
      '#documents' => array($object->id),
      '#sources' => array(),
    ),
  );
}

/**
 * Show the note version of the CWRC-Writer.
 */
function islandora_cwrc_document_display_cwrc_writer_note() {
  drupal_goto('cwrc/editor/note.htm');
}

/**
 * Check that a file is well-formed xml.
 *
 * @param file $file
 *   A file object.
 *
 * @return bool
 *   Returns TRUE if xml is well-formed, or FALSE if it is not.
 */
function islandora_cwrc_document_check_xml($file) {
  // Check that the uploaded file is well-formed XML.
  libxml_use_internal_errors(TRUE);
  simplexml_load_file(drupal_realpath($file->uri));
  $errors = count(libxml_get_errors());
  libxml_use_internal_errors(FALSE);
  if ($errors) {
    return FALSE;
  }
  return TRUE;
}
