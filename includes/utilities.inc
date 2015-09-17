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
 * Wrapper for islandora_download_datastream() that modifies CWRC datastreams.
 *
 * @see islandora_download_datastream().
 */
function islandora_cwrc_document_download_datastream(AbstractDatastream $datastream) {
  islandora_cwrc_document_view_datastream($datastream, TRUE);
}

/**
 * Wrapper for islandora_view_datastream() that modifies CWRC datastreams.
 *
 * @see islandora_view_datastream().
 */
function islandora_cwrc_document_view_datastream(AbstractDatastream $datastream, $download = FALSE, $version = NULL) {
  $object = $datastream->parent;
  module_load_include('inc', 'islandora', 'includes/datastream');
  if ($datastream->id == 'CWRC' && in_array('cwrc:documentCModel', $object->models)) {
    $accessibles = islandora_cwrc_document_get_rng_css_accessibility($object);
    if ($accessibles) {
      // Get and possibly modify the original content.
      if ($datastream->controlGroup == 'X' || 'M') {
        global $base_url;
        $cwrc = $datastream->content;
        module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');

        // Attempt to add the two datastreams.
        foreach ($accessibles as $accessible) {
          $url = $object[$accessible]->controlGroup == 'X' || 'M' ?
            "$base_url/islandora/object/{$object->id}/datastream/$accessible/view" :
            $object[$accessible]->url;
          if ($accessible == 'SCHEMA') {
            $cwrc = islandora_cwrc_writer_add_model_processing_instruction_to_string($cwrc, $url);
          }
          else {
            $cwrc = islandora_cwrc_writer_add_stylesheet_processing_instruction_to_string($cwrc, $url, 'text/css');
          }
          // If something goes wrong, bail and defer to the original.
          if (!$cwrc) {
            $operation = $download ? 'download' : 'view';
            watchdog('islandora_cwrc_writer',
              'The !accessible datastream on !pid could not be added to the CWRC datastream when attempting to !operation it, as it created malformed XML; deferring to the unmodified datastream.',
              array(
                '!accessible' => $accessible,
                '!pid' => $object->id,
                '!operation' => $operation,
              ),
              WATCHDOG_WARNING
            );
            islandora_view_datastream($datastream, $download, $version);
          }
        }
        $modified = file_unmanaged_save_data($cwrc, 'temporary://');

        // Construct a dummy datastream. This won't be ingested, but is rather
        // just passed on to islandora_view_datastream() in lieu of the regular
        // one so we can reuse that code.
        $download_version = $object->constructDatastream('CWRC_DOWNLOADABLE');
        $download_version->label = $datastream->label;
        $download_version->state = $datastream->state;
        $download_version->mimetype = $datastream->mimetype;
        $download_version->createdDate = $datastream->createdDate;
        $download_version->setContentFromFile($modified);
        // No size data exists until the datastream is ingested, so set it now.
        $download_version->size = filesize($modified);
        unlink($modified);
        islandora_view_datastream($download_version, $download, $version);
      }
    }
  }
  // Default in all other cases.
  islandora_view_datastream($datastream, $download, $version);
}

/**
 * Helper to only check the accessibility of these datastreams once.
 *
 * @param AbstractObject $object
 *   The object the datastreams should exist on.
 *
 * @return array
 *   An array containing the IDs of accessible datastreams.
 */
function islandora_cwrc_document_get_rng_css_accessibility($object) {
  global $user;
  $results = array();
  foreach (array('CSS', 'SCHEMA') as $datastream) {
    if (isset($object[$datastream]) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object[$datastream], $user)) {
      $results[] = $datastream;
    }
  }
  return $results;

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
