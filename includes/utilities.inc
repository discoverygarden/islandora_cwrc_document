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
 * @param AbstractObject $object
 *   An object to be edited.
 * @param array $config
 *   Any configuration options to be merged with the default settings.
 *
 * @return array
 *   An array to be rendered.
 */
function islandora_cwrc_document_display_cwrc_writer($form, $form_state, AbstractObject $object, array $config = array()) {
  module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/breadcrumb');
  drupal_set_title($object->label);
  drupal_set_breadcrumb(islandora_get_breadcrumbs($object));
  form_load_include($form_state, 'inc', 'islandora_object_lock', 'includes/form_handlers');
  if (isset($config['initial_mode']) && $config['initial_mode'] != 'read') {
    islandora_object_lock_acquire_during_alter($form, $form_state, $object, 'CWRC', NULL, TRUE, "islandora/object/{$object->id}");
  }

  return array(
    'cwrc_writer' => array(
      '#theme' => 'islandora_cwrc_writer',
      '#object' => $object,
      '#schema' => islandora_cwrc_writer_get_document_schema_pid($object),
      '#documents' => array($object->id),
      '#sources' => array(),
      '#config' => $config,
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
 * @see islandora_download_datastream()
 */
function islandora_cwrc_document_download_datastream(AbstractDatastream $datastream) {
  islandora_cwrc_document_view_datastream($datastream, TRUE);
}

/**
 * Wrapper for islandora_view_datastream() that styles CWRC datastreams.
 *
 * @see islandora_view_datastream()
 */
function islandora_cwrc_document_view_datastream(AbstractDatastream $datastream, $download = FALSE, $version = NULL) {
  module_load_include('inc', 'islandora', 'includes/datastream');
  module_load_include('inc', 'islandora_cwrc_writer', 'includes/document');

  try {
    $result = islandora_cwrc_writer_style_document($datastream);

    // Otherwise, make a temp datastream and serve the styled contents.
    $object = $datastream->parent;
    $modified = file_unmanaged_save_data($result, 'temporary://');

    // Construct a dummy datastream. This won't be ingested, but is rather
    // just passed on to islandora_view_datastream() in lieu of the
    // regular one so we can reuse that code.
    $download_version = $object->constructDatastream('CWRC_DOWNLOADABLE');
    $download_version->label = $datastream->label;
    $download_version->state = $datastream->state;
    $download_version->mimetype = $datastream->mimetype;
    $download_version->createdDate = $datastream->createdDate;
    $download_version->setContentFromFile($modified);
    // No size data exists until the datastream is ingested, so set it
    // now.
    $download_version->size = filesize($modified);
    unlink($modified);
    islandora_view_datastream($download_version, $download);
  }
  catch (InvalidArgumentException $e) {
    islandora_view_datastream($datastream, $download, $version);
  }
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
  $file_contents = file_get_contents(drupal_realpath($file->uri));
  // XXX: Remove xml-model tag for validation as it will trigger a libxml error
  // depending on the compiled libxml2 version.
  $file_contents = preg_replace('/<\?xml-model .*\?>/', '', $file_contents);
  simplexml_load_string($file_contents);
  $errors = count(libxml_get_errors());
  libxml_use_internal_errors(FALSE);
  if ($errors) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Gets an array of stock templates from the xml/templates folder.
 *
 * @return array
 *   An array containing multiple stock template profiles, using the form:
 *   - 'pid' for the destination PID of the template,
 *   - 'label' for the template label, and
 *   - 'filename' for the name of the template XML file.
 */
function islandora_cwrc_document_get_stock_templates() {
  return array(
    array(
      'pid' => 'cwrc:biographyTemplate',
      'label' => 'Biography (Template)',
      'filename' => "biography.xml",
    ),
    array(
      'pid' => 'cwrc:blankTEIDocument',
      'label' => 'Blank TEI Document',
      'filename' => "blank_TEI_document.xml",
    ),
    array(
      'pid' => 'cwrc:eventTemplate',
      'label' => 'Event',
      'filename' => "event.xml",
    ),
    array(
      'pid' => 'cwrc:letterTemplate',
      'label' => 'Letter (Template)',
      'filename' => "letter.xml",
    ),
    array(
      'pid' => 'cwrc:poemTemplate',
      'label' => 'Poem (Template)',
      'filename' => "poem.xml",
    ),
    array(
      'pid' => 'cwrc:proseTemplate',
      'label' => 'Prose',
      'filename' => "prose.xml",
    ),
    array(
      'pid' => 'cwrc:sampleBiography',
      'label' => 'Biography (Sample)',
      'filename' => 'sample_biography.xml',
    ),
    array(
      'pid' => 'cwrc:sampleLetter',
      'label' => 'Letter (Sample)',
      'filename' => 'sample_letter.xml',
    ),
    array(
      'pid' => 'cwrc:samplePoem',
      'label' => 'Poem (Sample)',
      'filename' => 'sample_poem.xml',
    ),
    array(
      'pid' => 'cwrc:sampleWriting',
      'label' => 'Writing (Sample)',
      'filename' => 'sample_writing.xml',
    ),
    array(
      'pid' => 'cwrc:writingTemplate',
      'label' => 'Writing (Template)',
      'filename' => 'writing.xml',
    ),
  );
}

/**
 * Gets template objects with TEMPLATE datastreams as select form options.
 *
 * @return array
 *   An associative array pairing template PIDs with their labels.
 */
function islandora_cwrc_document_get_templates() {
  $content_model = ISLANDORA_CWRC_DOCUMENT_TEMPLATE_CONTENT_MODEL;
  $query = <<<EOQ
SELECT ?pid ?label
FROM <#ri>
WHERE {
  ?pid <fedora-model:hasModel> <info:fedora/$content_model> .
  ?pid <fedora-model:label> ?label .
  ?ds  <fedora-view:disseminationType> <info:fedora/*/TEMPLATE> .
  ?pid <info:fedora/fedora-system:def/view#disseminates> ?ds
}
EOQ;
  $connection = islandora_get_tuque_connection();
  $results = $connection->repository->ri->sparqlQuery($query);
  $templates = array();
  foreach ($results as $result) {
    $templates[$result['pid']['value']] = $result['label']['value'];
  }
  asort($templates);
  return $templates;
}
