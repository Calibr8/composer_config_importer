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
      'type' => [
        'description' => 'The type of config: feature or config.',
        'default-value' => 'config',
      ],
    ],
  ];
  return $items;
}

/**
 * Perform import.
 */
function drush_composer_config_importer($package = NULL) {

  // Determine source directory.
  if ($package) {

    $type = drush_get_option('type', 'config');
    $subdir = drush_get_option('subdir', 'config/install');
    if ($type == 'feature') {
      $location = drush_get_option('location', 'modules/features');
    }
    else {
      $location = drush_get_option('location', '../config-import');
    }

    if (substr($location, -1) != "/") $location .= "/";
    if (substr($subdir, 0, 1) != "/") $subdir = "/" . $subdir;
    $source_dir = $location . $package . $subdir;
    $package_dir = $location . $package;

  }
  else {
    return drush_log(dt('You must provide a package'), LogLevel::ERROR);
  }

  // enable dependencies

  $infoyaml = $package_dir . '/' . $package . '.info.yml';
  $file_contents = file_get_contents($infoyaml);
  $ymldata = Yaml::decode($file_contents);
  $extensions = $ymldata['dependencies'];
  $extension_info = drush_get_extensions(FALSE);
  $modules = array();
  foreach ($extensions as $name) {
    if (!$extension_info[$name]->status) {
      $modules[] = $name;
    }
  }
  if (count($modules)) {
    drush_print(dt('The following extensions will be enabled: !extensions', array('!extensions' => implode(', ', $modules))));
    if(!drush_confirm(dt('Do you really want to continue?'))) {
      return drush_user_abort();
    }

    \Drupal::service('module_installer')->install($modules);
  }


  // import config

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