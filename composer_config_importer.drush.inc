<?php

/**
 * @file
 * Contains turbo-charged config commands for a better partial workflow.
 */
use Drupal\Component\Serialization\Exception\InvalidDataTypeException;
use Drupal\Component\Serialization\Yaml;
use Drupal\config\StorageReplaceDataWrapper;
use Drupal\Core\Config\FileStorage;
use Drupal\Core\Config\StorageComparer;
use Drush\Config\StorageWrapper;
use Drush\Log\LogLevel;

/**
 * Implements hook_drush_command().
 */
function composer_config_importer_drush_command() {
  $deps = ['drupal dependencies' => ['config']];
  $items['composer-config-importer'] = [
    'description' => 'Import configuration added by composer.',
    'core' => ['8+'],
    'aliases' => ['cci'],
    'arguments' => [
       'package' => 'The name of the config package to import.',
     ],
    'options' => [
      'location' => [
        'description' => 'Where the imported config package is located.',
        'example-value' => '../config-import',
      ],
      'subdir' => [
        'description' => 'The subdirectory the yml files are in.',
        'example-value' => 'config',
      ],
    ],
  ];
  return $items;
}

/**
 * Perform import.
 */
function drush_composer_config_importer($package = NULL) {

  drush_print(dt('Starting Import.'));

  // Determine source directory.
  if ($package) {
    $location = drush_get_option('location', '../config-import');
    if (substr($location, -1) != "/") $location .= "/";
    $subdir = drush_get_option('subdir', 'config');
    if (substr($subdir, 0, 1) != "/") $subdir = "/" . $subdir;
    $source_dir = $location . $package . $subdir;
  }
  else {
    // @todo get source from config
    return drush_log(dt('You must provide a package'), LogLevel::ERROR);
  }

  /** @var \Drupal\Core\Config\StorageInterface $active_storage */
  $active_storage = \Drupal::service('config.storage');
  $source_storage = new StorageReplaceDataWrapper($active_storage);
  $file_storage = new FileStorage($source_dir);
  foreach ($file_storage->listAll() as $name) {
    $data = $file_storage->read($name);
    $source_storage->replaceData($name, $data);
  }

  /** @var \Drupal\Core\Config\ConfigManagerInterface $config_manager */
  $config_manager = \Drupal::service('config.manager');
  $storage_comparer = new StorageComparer($source_storage, $active_storage, $config_manager);

  if (!$storage_comparer->createChangelist()->hasChanges()) {
    return drush_log(dt('There are no changes to import.'), LogLevel::OK);
  }

  $change_list = array();
  foreach ($storage_comparer->getAllCollectionNames() as $collection) {
    $change_list[$collection] = $storage_comparer->getChangelist(NULL, $collection);
  }
  _drush_print_config_changes_table($change_list);

  if (drush_confirm(dt('Import the listed configuration changes?'))) {
    return drush_op('_drush_config_import', $storage_comparer);
  }

}