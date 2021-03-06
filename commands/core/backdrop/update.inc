<?php
/**
 * @file
 *   Update.php for provisioned sites.
 *   This file is a derivative of the standard backdrop update.php,
 *   which has been modified to allow being run from the command
 *   line.
 */

/**
 * Global flag to identify update.php run, and so avoid various unwanted
 * operations, such as hook_init() and hook_exit() invokes, css/js preprocessing
 * and translation, and solve some theming issues. This flag is checked on several
 * places in Backdrop code (not just update.php).
 */
define('MAINTENANCE_MODE', 'update');

/**
 * Backdrop's update.inc has functions that are in previous update_X.inc files
 * for example, update_check_incompatibility() which can prove useful when
 * enabling modules.
 */
require_once BACKDROP_ROOT . '/core/includes/update.inc';
/**
 * Returns (and optionally stores) extra requirements that only apply during
 * particular parts of the update.php process.
 */
function update_extra_requirements($requirements = NULL) {
  static $extra_requirements = array();
  if (isset($requirements)) {
    $extra_requirements += $requirements;
  }
  return $extra_requirements;
}

/**
 * Perform one update and store the results which will later be displayed on
 * the finished page.
 *
 * An update function can force the current and all later updates for this
 * module to abort by returning a $ret array with an element like:
 * $ret['#abort'] = array('success' => FALSE, 'query' => 'What went wrong');
 * The schema version will not be updated in this case, and all the
 * aborted updates will continue to appear on update.php as updates that
 * have not yet been run.
 *
 * @param $module
 *   The module whose update will be run.
 * @param $number
 *   The update number to run.
 * @param $context
 *   The batch context array
 */
function brush_update_do_one($module, $number, $dependency_map,  &$context) {
  $function = $module . '_update_' . $number;

  // If this update was aborted in a previous step, or has a dependency that
  // was aborted in a previous step, go no further.
  if (!empty($context['results']['#abort']) && array_intersect($context['results']['#abort'], array_merge($dependency_map, array($function)))) {
    return;
  }


  $context['log'] = FALSE;

  $ret = array();
  if (function_exists($function)) {
    try {
      if ($context['log']) {
        Database::startLog($function);
      }

      brush_log("Executing " . $function);
      $ret['results']['query'] = $function($context['sandbox']);
      $ret['results']['success'] = TRUE;
    }
    // @TODO We may want to do different error handling for different exception
    // types, but for now we'll just print the message.
    catch (Exception $e) {
      $ret['#abort'] = array('success' => FALSE, 'query' => $e->getMessage());
      brush_set_error('BACKDROP_EXCEPTION', $e->getMessage());
    }

    if ($context['log']) {
      $ret['queries'] = Database::getLog($function);
    }
  }

  if (isset($context['sandbox']['#finished'])) {
    $context['finished'] = $context['sandbox']['#finished'];
    unset($context['sandbox']['#finished']);
  }

  if (!isset($context['results'][$module])) {
    $context['results'][$module] = array();
  }
  if (!isset($context['results'][$module][$number])) {
    $context['results'][$module][$number] = array();
  }
  $context['results'][$module][$number] = array_merge($context['results'][$module][$number], $ret);

  if (!empty($ret['#abort'])) {
    // Record this function in the list of updates that were aborted.
    $context['results']['#abort'][] = $function;
  }

  // Record the schema update if it was completed successfully.
  if ($context['finished'] == 1 && empty($ret['#abort'])) {
    backdrop_set_installed_schema_version($module, $number);
  }

  $context['message'] = 'Updating ' . check_plain($module) . ' module';
}



/**
 * Check update requirements and report any errors.
 */
function update_check_requirements() {
  $warnings = FALSE;

  // Check the system module and update.php requirements only.
  $requirements = system_requirements('update');
  $requirements += update_extra_requirements();

  // If there are issues, report them.
  foreach ($requirements as $requirement) {
    if (isset($requirement['severity']) && $requirement['severity'] != REQUIREMENT_OK) {
      $message = isset($requirement['description']) ? $requirement['description'] : '';
      if (isset($requirement['value']) && $requirement['value']) {
        $message .= ' (Currently using ' . $requirement['title'] . ' ' . $requirement['value'] . ')';
      }
      $warnings = TRUE;
      backdrop_set_message($message, 'warning');
    }
  }
  return $warnings;
}


function update_main_prepare() {
  // Some unavoidable errors happen because the database is not yet up-to-date.
  // Our custom error handler is not yet installed, so we just suppress them.
  brush_errors_off();


  // We prepare a minimal bootstrap for the update requirements check to avoid
  // reaching the PHP memory limit.
  require_once BACKDROP_ROOT . '/core/includes/bootstrap.inc';
  require_once BACKDROP_ROOT . '/core/includes/common.inc';
  require_once BACKDROP_ROOT . '/core/includes/file.inc';
  include_once BACKDROP_ROOT . '/core/includes/unicode.inc';

  backdrop_bootstrap(BACKDROP_BOOTSTRAP_SESSION);

  require_once BACKDROP_ROOT . '/core/includes/install.inc';
  require_once BACKDROP_ROOT . '/core/modules/system/system.install';

  // Load module basics.
  include_once BACKDROP_ROOT . '/core/includes/module.inc';
  $module_list['system']['filename'] = 'core/modules/system/system.module';
  module_list(TRUE, FALSE, FALSE, $module_list);
  backdrop_load('module', 'system');

  // Reset the module_implements() cache so that any new hook implementations
  // in updated code are picked up.
  module_implements('', FALSE, TRUE);

  // Set up $language, since the installer components require it.
  backdrop_language_initialize();

  // Set up theme system for the maintenance page.
  backdrop_maintenance_theme();

  // Check the update requirements for Backdrop.
  update_check_requirements();

  // So bootstrap to BACKDROP_BOOTSTRAP_LANGUAGE then include unicode.inc.
  backdrop_bootstrap(BACKDROP_BOOTSTRAP_LANGUAGE);

  // Now proceed with a full bootstrap.

  brush_bootstrap(BRUSH_BOOTSTRAP_BACKDROP_FULL);
  backdrop_maintenance_theme();

  brush_errors_on();

  include_once BACKDROP_ROOT . '/core/includes/batch.inc';
  backdrop_load_updates();

  update_fix_compatibility();

   // Change query-strings on css/js files to enforce reload for all users.
  _backdrop_flush_css_js();
  // Flush the cache of all data for the update status module.
  if (db_table_exists('cache_update')) {
    cache_clear_all('*', 'cache_update', TRUE);
  }

  module_list(TRUE, FALSE, TRUE);
}



function update_main() {
  update_main_prepare();

  $pending = update_get_update_list();


  $start = array();

  // Ensure system module's updates run first
  $start['system'] = array();


  // Print a list of pending updates for this module and get confirmation.
  if (sizeof($pending)) {
    brush_print(dt('The following updates are pending:'));
    brush_print();
    foreach ($pending as $module => $updates) {
      if (isset($updates['start']))  {
        brush_print($module . ' module : ');
        if (isset($updates['start'])) {
          $start[$module] = $updates['start'];
          foreach ($updates['pending'] as $update) {
            brush_print($update, 2);
          }
        }
        brush_print();
      }
    }

    if (!brush_confirm(dt('Do you wish to run all pending updates?'))) {
      return brush_user_abort();
    }

    brush_update_batch($start);
  }
  else {
    brush_log(dt("No database updates required"), 'success');
  }

}

function _update_batch_command($id) {
  update_main_prepare();
  brush_batch_command($id);
}

/**
 * Start the database update batch process.
 *
 * @param $start
 *   An array of all the modules and which update to start at.
 * @param $redirect
 *   Path to redirect to when the batch has finished processing.
 * @param $url
 *   URL of the batch processing page (should only be used for separate
 *   scripts like update.php).
 * @param $batch
 *   Optional parameters to pass into the batch API.
 * @param $redirect_callback
 *   (optional) Specify a function to be called to redirect to the progressive
 *   processing page.
 */
function brush_update_batch($start) {
  // Resolve any update dependencies to determine the actual updates that will
  // be run and the order they will be run in.
  $updates = update_resolve_dependencies($start);

  // Store the dependencies for each update function in an array which the
  // batch API can pass in to the batch operation each time it is called. (We
  // do not store the entire update dependency array here because it is
  // potentially very large.)
  $dependency_map = array();
  foreach ($updates as $function => $update) {
    $dependency_map[$function] = !empty($update['reverse_paths']) ? array_keys($update['reverse_paths']) : array();
  }

  $operations = array();
  foreach ($updates as $update) {
    if ($update['allowed']) {
      // Set the installed version of each module so updates will start at the
      // correct place. (The updates are already sorted, so we can simply base
      // this on the first one we come across in the above foreach loop.)
      if (isset($start[$update['module']])) {
        backdrop_set_installed_schema_version($update['module'], $update['number'] - 1);
        unset($start[$update['module']]);
      }
      // Add this update function to the batch.

      $function = $update['module'] . '_update_' . $update['number'];
      $operations[] = array('brush_update_do_one', array($update['module'], $update['number'], $dependency_map[$function]));
    }
  }

  $batch['operations'] = $operations;
  $batch += array(
    'title' => 'Updating',
    'init_message' => 'Starting updates',
    'error_message' => 'An unrecoverable error has occurred. You can find the error message below. It is advised to copy it to the clipboard for reference.',
    'finished' => 'brush_update_finished',
    'file' => 'includes/update.inc',
  );
  batch_set($batch);
  brush_backend_batch_process('updatedb-batch-process');
}



function brush_update_finished($success, $results, $operations) {
  // Nothing to do here. All caches already cleared. Kept as documentation of 'finished' callback.
}
