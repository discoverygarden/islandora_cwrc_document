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
 * @param array $config
 *   Any configuration options to be merged with the default settings.
 *
 * @return array
 *   An array to be rendered.
 */
function islandora_cwrc_document_display_cwrc_writer(AbstractObject $object, array $config = array()) {
  module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/breadcrumb');
  drupal_set_title($object->label);
  drupal_set_breadcrumb(islandora_get_breadcrumbs($object));
  return theme('islandora_cwrc_writer', array(
    'object' => $object,
    'schema' => islandora_cwrc_writer_get_document_schema_pid($object),
    'documents' => array($object->id),
    'sources' => array(),
    'config' => $config,
  ));
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
 * Wrapper for islandora_view_datastream() that modifies CWRC datastreams.
 *
 * @see islandora_view_datastream()
 */
function islandora_cwrc_document_view_datastream(AbstractDatastream $datastream, $download = FALSE, $version = NULL) {
  $object = $datastream->parent;
  module_load_include('inc', 'islandora', 'includes/datastream');

  if ($datastream->id == 'CWRC' && in_array('cwrc:documentCModel', $object->models)) {
    // Get the schema object. If there's a problem, bail.
    $schema_pid = $object->relationships->get('http://cwrc.ca/ns/cw#', 'hasSchema');
    if ($schema_pid) {
      $operation = $download ? 'download' : 'view';
      $schema_object = islandora_object_load($schema_pid[0]['object']['value']);
      if (!$schema_object) {
        watchdog('islandora_cwrc_writer',
          'The hasSchema relationship on !object pointed to an object (!schema_object) that could not be loaded when attempting to !operation it; deferring to the unmodified datastream.',
          array(
            '!object' => $object->id,
            '!schema_object' => $schema_object->id,
            '!operation' => $operation,
          ),
          WATCHDOG_WARNING
        );
        islandora_view_datastream($datastream, $download, $version);
      }

      $accessibles = islandora_cwrc_document_get_rng_css_accessibility($schema_object);
      if ($accessibles) {
        // Get and possibly modify the original content.
        if ($datastream->controlGroup == 'X' || 'M') {
          global $base_url;
          $cwrc = $datastream->content;
          module_load_include('inc', 'islandora_cwrc_writer', 'includes/utilities');

          // Attempt to add the two datastreams.
          foreach ($accessibles as $accessible) {
            $url = $schema_object[$accessible]->controlGroup == 'X' || 'M' ?
              "$base_url/islandora/object/{$schema_object->id}/datastream/$accessible/view" :
              $schema_object[$accessible]->url;
            if ($accessible == 'SCHEMA') {
              $cwrc = islandora_cwrc_writer_add_model_processing_instruction_to_string($cwrc, $url);
            }
            else {
              $cwrc = islandora_cwrc_writer_add_stylesheet_processing_instruction_to_string($cwrc, $url, 'text/css');
            }
            // If something goes wrong, bail and defer to the original.
            if (!$cwrc) {
              watchdog('islandora_cwrc_writer',
                'The !accessible datastream on !pid could not be added to the CWRC datastream when attempting to !operation it as it created malformed XML; deferring to the unmodified datastream.',
                array(
                  '!accessible' => $accessible,
                  '!pid' => $schema_object->id,
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
