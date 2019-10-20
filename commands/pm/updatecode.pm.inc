<?php

/**
 * Command callback. Displays update status info and allows to update installed
 * projects.
 * Pass specific projects as arguments, otherwise we update all that have
 * candidate releases.
 *
 * This command prompts for confirmation before updating, so it is safe to run
 * just to check on. In this case, say at the confirmation prompt.
 */
function drop_pm_updatecode() {
  // We don't provide for other options here, so we supply an explicit path.
  drop_include_engine('update_info', 'backdrop', NULL, DROP_BASE_PATH . '/commands/pm/update_info');

  // Find only security updates?
  $security_only = drop_get_option('security-only');

  // Get specific requests.
  $requests = _convert_csv_to_array(func_get_args());

  // Parse out project name and version.
  $requests = pm_parse_project_version($requests);

  // Get installed extensions and projects.
  $extensions = drop_get_extensions();
  $projects = drop_get_projects($extensions);

  // Get update status information.
  $update_info = _pm_get_update_info($projects);

  // Process locks specified on the command line.
  $locked_list = drop_pm_update_lock($update_info, drop_get_option_list('lock'), drop_get_option_list('unlock'), drop_get_option('lock-message'));

  foreach ($extensions as $name => $extension) {
    // Add an item to $update_info for each enabled extension which was obtained
    // from cvs or git and its project is unknown (because of cvs_deploy or
    // git_deploy is not enabled).
    if (!isset($extension->info['project'])) {
      if ((isset($extension->vcs)) && ($extension->status)) {
        $update_info[$name] = array(
          'title' => $extension->info['name'],
          'existing_version' => 'Unknown',
          'status' => DROP_PM_REQUESTED_PROJECT_NOT_PACKAGED,
          'status_msg' => dt('Project was not packaged by backdrop.org but obtained from !vcs. You need to enable !vcs_deploy module', array('!vcs' => $extension->vcs))
        );
        // The user may have requested to update a project matching this
        // extension. If it was by coincidence or error we don't mind as we've
        // already added an item to $update_info. Just clean up $requests.
        if (isset($requests[$name])) {
          unset($requests[$name]);
        }
      }
    }
    // Aditionally if the extension name is distinct to the project name and
    // the user asked to update the extension, fix the request.
    elseif ((isset($requests[$name])) && ($extension->name != $extension->info['project'])) {
      $requests[$extension->info['project']] = $requests[$name];
      unset($requests[$name]);
    }
  }

  // Add an item to $update_info for each request not present in $update_info.
  foreach ($requests as $name => $request) {
    if (!isset($update_info[$name])) {
      // Disabled projects.
      if ((isset($projects[$name])) && ($projects[$name]['status'] == 0)) {
        $update_info[$name] = array(
          'title' => $name,
          'existing_version' => $projects[$name]['version'],
          'status' => DROP_PM_REQUESTED_PROJECT_NOT_UPDATEABLE
        );
        unset($requests[$name]);
      }
      // At this point we are unable to find matching installed project.
      // It does not exist at all or it is mispelled,...
      else {
        $update_info[$name] = array(
          'title' => $name,
          'existing_version' => 'Unknown',
          'status'=> DROP_PM_REQUESTED_PROJECT_NOT_FOUND,
        );
      }
    }
  }

  // If specific versions were requested, match the requested release.
  foreach ($requests as $name => $request) {
    if (!empty($request['version'])) {
      $release = pm_get_release($request, $update_info[$name]);
      if (!$release) {
        $update_info[$name]['status'] = DROP_PM_REQUESTED_VERSION_NOT_FOUND;
      }
      else if ($release['version'] == $update_info[$name]['existing_version']) {
        $update_info[$name]['status'] = DROP_PM_REQUESTED_CURRENT;
      }
      else {
        $update_info[$name]['status'] = DROP_PM_REQUESTED_UPDATE;
      }
      // Set the candidate version to the requested release.
      $update_info[$name]['candidate_version'] = $release['version'];
    }
  }

  // Table headers.
  $rows[] = array(dt('Name'), dt('Installed version'), dt('Proposed version'), dt('Status'));

  // Process releases, notifying user of status and
  // building a list of proposed updates.
  $updateable = pm_project_filter($update_info, $rows, $security_only);

  // Pipe preparation.
  if (drop_get_context('DROP_PIPE')) {
    $pipe = "";
    foreach($updateable as $project) {
      $pipe .= $project['name']. " ";
      $pipe .= $project['existing_version']. " ";
      $pipe .= $project['candidate_version']. " ";
      $pipe .= str_replace(' ', '-', pm_update_filter($project)). "\n";
    }
    drop_print_pipe($pipe);
    // Automatically curtail update process if in pipe mode.
    $updateable = array();
  }

  $tmpfile = drop_tempnam('pm-updatecode.');

  $last = pm_update_last_check();
  drop_print(dt('Update information last refreshed: ') . ($last  ? format_date($last) : dt('Never')));
  drop_print();
  drop_print(dt("Update status information on all installed and enabled Backdrop projects:"));
  drop_print_table($rows, TRUE, array(3 => 40), $tmpfile);
  $contents = file_get_contents($tmpfile);
  drop_print($contents);
  drop_print();

  // If specific project updates were requested then remove releases for all
  // others.
  if (!empty($requests)) {
    foreach ($updateable as $name => $project) {
      if (!isset($requests[$name])) {
        unset($updateable[$name]);
      }
    }
  }

  // Prevent update of core if --no-core was specified.
  if (isset($updateable['backdrop']) && drop_get_option('no-core', FALSE)) {
    unset($updateable['backdrop']);
    drop_print(dt('Skipping core update (--no-core specified).'));
  }

  // If there are any locked projects that were not requested, then remove them.
  if (!empty($locked_list)) {
    foreach ($updateable as $name => $project) {
      if ((isset($locked_list[$name])) && (!isset($requests[$name]))) {
        unset($updateable[$name]);
      }
    }
  }

  // First check to see if there is a newer drop.
  $drop_update_available = NULL;
  if (drop_get_option('self-update', TRUE)) {
    $drop_update_available = drop_check_self_update();
  }

  // Do no updates in simulated mode.
  if (drop_get_context('DROP_SIMULATE')) {
    return drop_log(dt('No action taken in simulated mode.'), 'ok');
    return TRUE;
  }

  $core_update_available = FALSE;
  if (isset($updateable['backdrop'])) {
    $backdrop_project = $updateable['backdrop'];
    unset($update_info['backdrop']);
    unset($updateable['backdrop']);

    // At present we need to update backdrop core after non-core projects
    // are updated.
    if (empty($updateable)) {
      return _pm_update_core($backdrop_project, $tmpfile);
    }
    // If there are modules other than backdrop core enabled, then update them
    // first.
    else {
      $core_update_available = TRUE;
      if ($backdrop_project['status'] == UPDATE_NOT_SECURE) {
        drop_print(dt("NOTE: A security update for the Backdrop core is available."));
      }
      else {
        drop_print(dt("NOTE: A code update for the Backdrop core is available."));
      }
      drop_print(dt("Backdrop core will be updated after all of the non-core modules are updated.\n"));
    }
  }

  // If there are no releases to update, then print a final
  // exit message.  Supress the message if we already printed
  // a message about a drop update being available.
  if (empty($updateable)) {
    if ($drop_update_available === TRUE) {
      return FALSE;
    }
    if ($security_only) {
      return drop_log(dt('No security updates available.'), 'ok');
    }
    else {
      return drop_log(dt('No code updates available.'), 'ok');
    }
  }

  // Offer to update to the identified releases.
  if (!pm_update_packages($updateable, $tmpfile)) {
    return FALSE;
  }

  // After projects are updated we can update core.
  if ($core_update_available) {
    drop_print();
    return _pm_update_core($backdrop_project, $tmpfile);
  }
}

/**
 * Update backdrop core, following interactive confirmation from the user.
 *
 * @param $project
 *   The backdrop project information from the backdrop.org update service,
 *   copied from $update_info['backdrop'].  @see drop_pm_updatecode.
 */
function _pm_update_core(&$project, $tmpfile) {
  drop_include_engine('package_handler', drop_get_option('package-handler', 'wget'));
  $backdrop_root = drop_get_context('DROP_BACKDROP_ROOT');

  drop_print(dt('Code updates will be made to backdrop core.'));
  drop_print(dt("WARNING:  Updating core will discard any modifications made to Backdrop core files, most noteworthy among these are .htaccess and robots.txt.  If you have made any modifications to these files, please back them up before updating so that you can re-create your modifications in the updated version of the file."));
  drop_print(dt("Note: Updating core can potentially break your site. It is NOT recommended to update production sites without prior testing."));
  drop_print();
  if(!drop_confirm(dt('Do you really want to continue?'))) {
    drop_print(dt('Rolling back all changes. Run again with --no-core to update modules only.'));
    return drop_user_abort();
  }

  // We need write permission on $backdrop_root.
  if (!is_writable($backdrop_root)) {
    return drop_set_error('DROP_PATH_NO_WRITABLE', dt('Backdrop root path is not writable.'));
  }

  // Create a directory 'core' if it does not already exist.
  $project['path'] = 'backdrop-' . $project['candidate_version'];
  $project['full_project_path'] = $backdrop_root . '/' . $project['path'];
  if (!is_dir($project['full_project_path'])) {
    drop_mkdir($project['full_project_path']);
  }

  // Create a list of directories to exclude from the update process.
  $skip_list = array('sites', $project['path']);
  // Add non-writable directories: we can't move them around.
  // We will also use $items_to_test later for $version_control check.
  $items_to_test = drop_scan_directory($backdrop_root, '/.*/', array_merge(array('.', '..'), $skip_list), 0, FALSE, 'basename', 0, TRUE);
  foreach (array_keys($items_to_test) as $item) {
    if (is_dir($item) && !is_writable($item)) {
      $skip_list[] = $item;
      unset($items_to_test[$item]);
    }
    elseif (is_link($item)) {
      $skip_list[] = $item;
      unset($items_to_test[$item]);
    }
  }
  $project['skip_list'] = $skip_list;

  // Move all files and folders in $backdrop_root to the new 'core' directory
  // except for the items in the skip list
  _pm_update_move_files($backdrop_root, $project['full_project_path'], $project['skip_list']);

  // Set a context variable to indicate that rollback should reverse
  // the _pm_update_move_files above.
  drop_set_context('DROP_PM_BACKDROP_CORE', $project);

  if (!$version_control = drop_pm_include_version_control($project['full_project_path'])) {
    return FALSE;
  }

  $project['base_project_path'] = dirname($project['full_project_path']);
  // Check we have a version control system, and it clears its pre-flight.
  if (!$version_control->pre_update($project, $items_to_test)) {
    return FALSE;
  }

  // Package handlers want the project directory in project_dir.
  $project['project_dir'] = $project['path'];

  // Update core.
  if (pm_update_project($project, $version_control) === FALSE) {
    return FALSE;
  }

  // Take the updated files in the 'core' directory that have been updated,
  // and move all except for the items in the skip list back to
  // the backdrop root
  _pm_update_move_files($project['full_project_path'], $backdrop_root, $project['skip_list']);
  drop_delete_dir($project['full_project_path']);

  // Version control engines expect full_project_path to exist and be accurate.
  $project['full_project_path'] = $project['base_project_path'];

  // If there is a backup target, then find items
  // in the backup target that do not exist at the
  // backdrop root.  These are to be moved back.
  if (array_key_exists('backup_target', $project)) {
    _pm_update_move_files($project['backup_target'], $backdrop_root, $project['skip_list'], FALSE);
    _pm_update_move_files($project['backup_target'] . '/profiles', $backdrop_root . '/profiles', array('default'), FALSE);
  }

  pm_update_complete($project, $version_control);

  return TRUE;
}

/**
 * Move some files from one location to another
 */
function _pm_update_move_files($src_dir, $dest_dir, $skip_list, $remove_conflicts = TRUE) {
  $items_to_move = drop_scan_directory($src_dir, '/.*/', array_merge(array('.', '..'), $skip_list), 0, FALSE, 'filename', 0, TRUE);
  foreach ($items_to_move as $filename => $info) {
    if ($remove_conflicts) {
      drop_delete_dir($dest_dir . '/' . basename($filename));
    }
    if (!file_exists($dest_dir . '/' . basename($filename))) {
      $move_result = drop_move_dir($filename,  $dest_dir . '/' . basename($filename));
    }
  }
  return TRUE;
}

/**
 * Update projects according to an array of releases for each project, 
 * following interactive confirmation from the user.
 *
 * @param $update_info
 *   An array of projects from the backdrop.org update service, with an additional
 *   array key candidate_version that specifies the version to be installed.
 */
function pm_update_packages($update_info, $tmpfile) {
  drop_include_engine('package_handler', drop_get_option('package-handler', 'wget'));
  $backdrop_root = drop_get_context('DROP_BACKDROP_ROOT');

  $print = '';
  $status = array();
  foreach($update_info as $project) {
    $print .= $project['title'] . " [" . $project['name'] . '-' . $project['candidate_version'] . "], ";
    $status[$project['status']] = $project['status'];
  }
  // We print the list of the projects that need to be updated.
  if (isset($status[UPDATE_NOT_SECURE])) {
    if (isset($status[UPDATE_NOT_CURRENT])) {
      $title = (dt('Security and code updates will be made to the following projects:'));
    }
    else {
      $title = (dt('Security updates will be made to the following projects:'));
    }
  }
  else {
    $title = (dt('Code updates will be made to the following projects:'));
  }
  $print = "$title " . (substr($print, 0, strlen($print)-2));
  drop_print($print);
  file_put_contents($tmpfile, "\n\n$print\n\n", FILE_APPEND);

  // We print some warnings before the user confirms the update.
  drop_print();
  if (drop_get_option('no-backup', FALSE)) {
    drop_print(dt("Note: You have selected to not store backups."));
  }
  else {
    drop_print(dt("Note: A backup of your project will be stored to backups directory if it is not managed by a supported version control system."));
    drop_print(dt('Note: If you have made any modifications to any file that belongs to one of these projects, you will have to migrate those modifications after updating.'));
  }
  if(!drop_confirm(dt('Do you really want to continue with the update process?'))) {
    return drop_user_abort();
  }

  // Now we start the actual updating.
  foreach($update_info as $project) {
    if (empty($project['path'])) {
      return drop_set_error('DROP_PM_UPDATING_NO_PROJECT_PATH', dt('The !project project path is not available, perhaps the !type is enabled but has been deleted from disk.', array('!project' => $project['name'], '!type' => $project['project_type'])));
    }
    drop_log(dt('Starting to update !project code at !dir...', array('!project' => $project['title'], '!dir' => $project['path'])));
    // Create the projects directory and base (parent) directory.
    $project['full_project_path'] = $backdrop_root . '/' . $project['path'];
    // Check that the directory exists, and is where we expect it to be.
    if (stripos($project['path'], $project['project_type']) === FALSE || !is_dir($project['full_project_path'])) {
      return drop_set_error('DROP_PM_UPDATING_PATH_NOT_FOUND', dt('The !project directory could not be found within the !types directory at !full_project_path, perhaps the project is enabled but has been deleted from disk.', array('!project' => $project['name'], '!type' => $project['project_type'], '!full_project_path' => $project['full_project_path'])));
    }
    if (!$version_control = drop_pm_include_version_control($project['full_project_path'])) {
      return FALSE;
    }
    $project['base_project_path'] = dirname($project['full_project_path']);
    // Check we have a version control system, and it clears its pre-flight.
    if (!$version_control->pre_update($project)) {
      return FALSE;
    }

    // Package handlers want the name of the directory in project_dir.
    // It may be different to the project name for pm-download.
    // Perhaps we want here filename($project['full_project_path']).
    $project['project_dir'] = $project['name'];

    // Run update on one project.
    if (pm_update_project($project, $version_control) === FALSE) {
      return FALSE;
    }
    pm_update_complete($project, $version_control);
  }

  return TRUE;
}

/**
 * Update one project -- a module, theme or Backdrop core.
 *
 * @param $project
 *   The project to upgrade.  $project['full_project_path'] must be set
 *   to the location where this project is stored.
 */
function pm_update_project($project, $version_control) {
  // 1. If the version control engine is a proper vcs we need to remove project
  // files in order to not have orphan files after update.
  // 2. If the package-handler is cvs or git, it will remove upstream removed
  // files and no orphans will exist after update.
  // So, we must remove all files previous update if the directory is not a
  // working copy of cvs or git but we don't need to remove them if the version
  // control engine is backup, as it did already move the project out to the
  // backup directory.
  if (($version_control->engine != 'backup') && (drop_get_option('package-handler', 'wget') == 'wget')) {
    // Find and unlink all files but the ones in the vcs control directories.
    $skip_list = array('.', '..');
    $skip_list = array_merge($skip_list, drop_version_control_reserved_files());
    drop_scan_directory($project['full_project_path'], '/.*/', $skip_list, 'unlink', TRUE, 'filename', 0, TRUE);
  }

  // Add the project to a context so we can roll back if needed.
  $updated = drop_get_context('DROP_PM_UPDATED');
  $updated[] = $project;
  drop_set_context('DROP_PM_UPDATED', $updated);

  if (!package_handler_update_project($project, $project['releases'][$project['candidate_version']])) {
    return drop_set_error('DROP_PM_UPDATING_FAILED', dt('Updating project !project failed. Attempting to roll back to previously installed version.', array('!project' => $project['name'])));
  }

  // If the version control engine is a proper vcs we also need to remove
  // orphan directories.
  if (($version_control->engine != 'backup') && (drop_get_option('package-handler', 'wget') == 'wget')) {
    $files = drop_find_empty_directories($project['full_project_path'], $version_control->reserved_files());
    array_map('drop_delete_dir', $files);
  }

  return TRUE;
}

/**
 * Run the post-update hooks after updatecode is complete for one project.
 */
function pm_update_complete($project, $version_control) {
  drop_print(dt('Project !project was updated successfully. Installed version is now !version.', array('!project' => $project['name'], '!version' => $project['candidate_version'])));
  drop_command_invoke_all('pm_post_update', $project['name'], $project['releases'][$project['candidate_version']]);
  $version_control->post_update($project);
}

function drop_pm_updatecode_rollback() {
  $projects = array_reverse(drop_get_context('DROP_PM_UPDATED', array()));
  foreach($projects as $project) {
    drop_log(dt('Rolling back update of !project code ...', array('!project' => $project['title'])));

    // Check we have a version control system, and it clears it's pre-flight.
    if (!$version_control = drop_pm_include_version_control($project['path'])) {
      return FALSE;
    }
    $version_control->rollback($project);
  }

  // Post rollback, we will do additional repair if the project is backdrop core.
  $backdrop_core = drop_get_context('DROP_PM_BACKDROP_CORE', FALSE);
  if ($backdrop_core) {
    $backdrop_root = drop_get_context('DROP_BACKDROP_ROOT');
    _pm_update_move_files($backdrop_core['full_project_path'], $backdrop_root, $backdrop_core['skip_list']);
    drop_delete_dir($backdrop_core['full_project_path']);
  }
}

/**
 * Return an array of updateable projects and fill $rows.
 *
 * Array of updateable projects is obtained from calculated project update
 * status and $security_only flag.
 */
function pm_project_filter(&$update_info, &$rows, $security_only) {
  $updateable = array();
  foreach ($update_info as $key => $project) {
    if (empty($project['title'])) {
      continue;
    }
    switch($project['status']) {
      case DROP_PM_REQUESTED_UPDATE:
        $status = dt('Specified version available');
        $project['updateable'] = TRUE;
        break;
      case DROP_PM_REQUESTED_CURRENT:
        $status = dt('Specified version already installed');
        break;
      case DROP_PM_REQUESTED_PROJECT_NOT_PACKAGED:
        $status = $project['status_msg'];
        break;
      case DROP_PM_REQUESTED_VERSION_NOT_FOUND:
        $status = dt('Specified version not found');
        break;
      case DROP_PM_REQUESTED_PROJECT_NOT_FOUND:
        $status = dt('Specified project not found');
        break;
      case DROP_PM_REQUESTED_PROJECT_NOT_UPDATEABLE:
        $status = dt('Project has no enabled extensions and can\'t be updated');
        break;
      default:
        $status = pm_update_filter($project);
        break;
    }

    // Special checking:  if drop decides that the candidate version is older
    // than the installed version, then we will set the candidate version to
    // the installed version.
    if (isset($project['candidate_version'], $project['releases'][$project['candidate_version']], $project['releases'][$project['existing_version']])) {
      if ($project['releases'][$project['candidate_version']]['date'] < $project['releases'][$project['existing_version']]['date']) {
        $project['candidate_version'] = $project['existing_version'];
      }
    }

    if (isset($project['locked'])) {
      $status = $project['locked'] . " ($status)";
    }

    // Persist candidate_version in $update_info (plural).
    if (empty($project['candidate_version'])) {
      $update_info[$key]['candidate_version'] = $project['existing_version']; // Default to no change
    }
    else {
      $update_info[$key]['candidate_version'] = $project['candidate_version'];
    }
    if (!empty($project['updateable'])) {
      $updateable[$key] = $project;
      // Find only security updates
      if ($security_only && ($project['status'] != UPDATE_NOT_SECURE)) {
        unset($updateable[$key]);
      }
    }
    $rows[] = array($project['title'], $project['existing_version'], $update_info[$key]['candidate_version'], $status);
  }
  return $updateable;
}

/**
 * Set a release to a recommended version (if available), and set as updateable.
 */
function pm_release_recommended(&$project) {
  if (isset($project['recommended'])) {
    $project['candidate_version'] = $project['recommended'];
    $project['updateable'] = TRUE;
  }
}

/**
 * Get the a best release match for a requested update.
 *
 * @param $request A information array for the requested project
 * @param $project A project information array for this project, as returned by an update service from pm_get_extensions()
 */
function pm_get_release($request, $project) {
  $minor = '';
  $version_patch_changed = '';
  if ($request['version']) {
    // The user specified a specific version - try to find that exact version
    foreach($project['releases'] as $version => $release) {
      // Ignore unpublished releases.
      if ($release['status'] != 'published') {
        continue;
      }

      // Straight match
      if (!isset($recommended_version) && $release['version'] == $request['version']) {
        $recommended_version = $version;
      }
    }
  }
  else {
    // No version specified - try to find the best version we can
    foreach($project['releases'] as $version => $release) {
      // Ignore unpublished releases.
      if ($release['status'] != 'published') {
        continue;
      }

      // If we haven't found a recommended version yet, put the dev
      // version as recommended and hope it gets overwritten later.
      // Look for the 'latest version' if we haven't found it yet.
      // Latest version is defined as the most recent version for the
      // default major version.
      if (!isset($latest_version) && $release['version_major'] == $project['default_major']) {
        $latest_version = $version;
      }

      if (!isset($recommended_version) && $release['version_major'] == $project['default_major']) {
        if ($minor != $release['version_patch']) {
          $minor = $release['version_patch'];
          $version_patch_changed = $version;
        }
        if (empty($release['version_extra']) && $minor == $release['version_patch']) {
          $recommended_version = $version_patch_changed;
        }
        continue;
      }
    }
  }
  if (isset($recommended_version)) {
    return $project['releases'][$recommended_version];
  }
  else if (isset($latest_version)) {
    return $project['releases'][$latest_version];
  }
  else {
    return false;
  }
}