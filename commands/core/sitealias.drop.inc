<?php

/**
 * @file
 *   Site alias commands. @see example.droprc.php for details.
 */

function sitealias_drop_help($section) {
  switch ($section) {
    case 'drop:site-alias':
      return dt('Print an alias record.');
  }
}

function sitealias_drop_command() {
  $items = array();

  $items['site-alias'] = array(
    'callback' => 'drop_sitealias_print',
    'description' => 'Print site alias records for all known site aliases and local sites.',
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'arguments' => array(
      'site' => 'Site specification alias to print',
    ),
    'options' => array(
      'full' => 'Print the full alias record for each site.  Default when aliases are specified on the command line.',
      'component' => 'Print only the specified element from the full alias record.',
      'short' => 'Print only the site alias name.  Default when not command line arguments are specified.',
      'pipe' => 'Print the long-form site specification for each site.',
      'with-db' => 'Include the databases structure in the full alias record.',
      'with-db-url' => 'Include the short-form db-url in the full alias record.',
      'no-db' => 'Do not include the database record in the full alias record (default).',
      'with-optional' => 'Include optional default items.',
    ),
    'aliases' => array('sa'),
    'examples' => array(
      'drop site-alias' => 'List all alias records known to drop.',
      'drop site-alias @dev' => 'Print an alias record for the alias \'dev\'.',
    ),
    'topics' => array('docs-aliases'),
  );
  return $items;
}

/**
 * Return a list of all site aliases known to drop.
 *
 * The array key is the site alias name, and the array value
 * is the site specification for the given alias.
 */
function _drop_sitealias_alias_list() {
  return drop_get_context('site-aliases');
}

/**
 * Return a list of all of the local sites at the current backdrop root.
 *
 * The array key is the site folder name, and the array value
 * is the site specification for that site.
 */
function _drop_sitealias_site_list() {
  $site_list = array();
  $base_path = drop_get_context('DROP_BACKDROP_ROOT') . '/sites';
  $files = drop_scan_directory($base_path, '/settings\.php/', array('.', '..', 'CVS', 'all'));
  foreach ($files as $filename => $info) {
    if ($info->basename == 'settings.php') {
      $alias_record = drop_sitealias_build_record_from_settings_file($filename);
      if (!empty($alias_record)) {
        $site_list[drop_sitealias_uri_to_site_dir($alias_record['uri'])] = $alias_record;
      }
    }
  }
  return $site_list;
}

/**
 * Return the list of all site aliases and all local sites.
 */
function _drop_sitealias_all_list() {
  drop_sitealias_load_all();
  return array_merge(_drop_sitealias_alias_list(), _drop_sitealias_site_list());
}

/**
 * Return the list of sites (aliases or local) that the
 * user specified on the command line.  If none were specified,
 * then all are returned.
 */
function _drop_sitealias_user_specified_list() {
  $command = drop_get_command();
  $specifications = $command['arguments'];
  $site_list = array();
  
  // Did the user specify --short or --full output?
  $specified_output_style = drop_get_option(array('full', 'short'), FALSE);
  
  // Iterate over the arguments and convert them to alias records
  if (!empty($specifications)) {
    $site_list = drop_sitealias_resolve_sitespecs($specifications);
    if (!$specified_output_style) {
      drop_set_option('full', TRUE);
    }
  }
  // If the user provided no args, then we will return everything.
  else {
    $site_list = _drop_sitealias_all_list();
    
    // Filter out the hidden items
    foreach ($site_list as $site_name => $one_site) {
      if (array_key_exists('#hidden', $one_site)) {
        unset($site_list[$site_name]); 
      }
    }
  }

  return $site_list;
}

/**
 * Print out the specified site aliases using the format
 * specified.
 */
function drop_sitealias_print() {
  // Call bootstrap max, unless the caller requested short output
  if (!drop_get_option('short', FALSE)) {
    drop_bootstrap_max();
  }
  
  $site_list = _drop_sitealias_user_specified_list();
  $full_output = drop_get_option('full');
  $long_output = drop_get_option('long');
  $with_db = (drop_get_option('with-db') != null) || (drop_get_option('with-db-url') != null);

  $site_specs = array();
  foreach ($site_list as $site => $alias_record) {
    if (!array_key_exists('site-list', $alias_record)) {
      $site_specs[] = drop_sitealias_alias_record_to_spec($alias_record, $with_db);
    }
    if (isset($full_output)) {
      $component = drop_get_option('component');
      if ($component) {
        if (array_key_exists($component, $alias_record)) {
	  drop_print($alias_record[$component]);
	}
	else {
	  drop_set_error('DROP_NO_SUCH_ELEMENT', dt('The element @component was not found in the alias record for @site.', array('@component' => $component, '@site' => $site)));
	}
      }
      else {
        _drop_sitealias_print_record($alias_record, $site);
      }
    }
    else {
      drop_print($site);
    }
  }
  drop_print_pipe(array_unique($site_specs));
}

/**
 * Given a site alias name, print out a php-syntax
 * representation of it.
 *
 * @param alias_record
 *   The name of the site alias to print
 */
function _drop_sitealias_print_record($alias_record, $site_alias = '') {
  $output_db = drop_get_option('with-db');
  $output_db_url = drop_get_option('with-db-url');
  $output_optional_items = drop_get_option('with-optional');

  // Make sure that the default items have been added for all aliases
  _drop_sitealias_add_static_defaults($alias_record);
    
  // Include the optional items, if requested
  if ($output_optional_items) {
    _drop_sitealias_add_transient_defaults($alias_record);
  }

  drop_sitealias_resolve_path_references($alias_record);

  if (isset($output_db_url)) {
    drop_sitealias_add_db_url($alias_record);
  }
  if (isset($output_db_url) || isset($output_db)) {
    drop_sitealias_add_db_settings($alias_record);
  }
  // If the user specified --with-db-url, then leave the
  // 'db-url' entry in the alias record (unless it is not
  // set, in which case we will leave the 'databases' record instead).
  if (isset($output_db_url)) {
    if (isset($alias_record['db-url'])) {
      unset($alias_record['databases']);
    }
  }
  // If the user specified --with-db, then leave the
  // 'databases' entry in the alias record.
  else if (isset($output_db)) {
    unset($alias_record['db-url']);
  }
  // If neither --with-db nor --with-db-url were specified,
  // then remove both the 'db-url' and the 'databases' entries.
  else {
    unset($alias_record['db-url']);
    unset($alias_record['databases']);
  }

  // The alias name will be the same as the site alias name,
  // unless the user specified some other name on the command line.
  $alias_name = drop_get_option('alias-name');
  if (!isset($alias_name)) {
    $alias_name = $site_alias;
    if (empty($alias_name) || is_numeric($alias_name)) {
      $alias_name = drop_sitealias_uri_to_site_dir($alias_record['uri']);
    }
  }

  // We don't want the name to go into the output
  unset($alias_record['#name']);
  unset($alias_record['#hidden']);

  // We only want to output the 'root' item; don't output the '%root' path alias
  if (array_key_exists('path-aliases', $alias_record) && array_key_exists('%root', $alias_record['path-aliases'])) {
    unset($alias_record['path-aliases']['%root']);
    // If there is nothing left in path-aliases, then clear it out
    if (count($alias_record['path-aliases']) == 0) {
      unset($alias_record['path-aliases']);
    }
  }
  
  // Alias names contain an '@' when referenced, but do
  // not contain an '@' when defined.
  if (substr($alias_name,0,1) == '@') {
    $alias_name = substr($alias_name,1);
  }

  if (!drop_get_option('show-passwords', FALSE)) {
    drop_unset_recursive($alias_record, 'password');
  }

  $exported_alias = var_export($alias_record, TRUE);
  drop_print('$aliases[\'' . $alias_name . '\'] = ' . $exported_alias . ';');
}

/**
 * Use heuristics to attempt to convert from a site directory to a URI.
 * This function should only be used when the URI really is unknown, as
 * the mapping is not perfect.
 *
 * @param site_dir
 *   A directory, such as domain.com.8080.backdrop
 *
 * @return string
 *   A uri, such as http://domain.com:8080/backdrop
 */
function _drop_sitealias_site_dir_to_uri($site_dir) {
  // Protect IP addresses NN.NN.NN.NN by converting them
  // temporarily to NN_NN_NN_NN for now.
  $uri = preg_replace("/([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)/", "$1_$2_$3_$4", $site_dir);
  // Convert .[0-9]+. into :[0-9]+/
  $uri = preg_replace("/\.([0-9]+)\./", ":$1/", $uri);
  // Convert .[0-9]$ into :[0-9]
  $uri = preg_replace("/\.([0-9]+)$/", ":$1", $uri);
  // Convert .(com|net|org|info). into .(com|net|org|info)/
  $uri = str_replace(array('.com.', '.net.', '.org.', '.info.'), array('.com/', '.net/', '.org/', '.info/'), $uri);

  // If there is a / then convert every . after the / to /
  // Then again, if we did this we would break if the path contained a "."
  // I hope that the path would never contain a "."...
  $pos = strpos($uri, '/');
  if ($pos !== false) {
    $uri = substr($uri, 0, $pos + 1) . str_replace('.', '/', substr($uri, $pos + 1));
  }

  // n.b. this heuristic works all the time if there is a port,
  // it also works all the time if there is a port and no path,
  // but it does not work for domains such as .co.jp with no path,
  // and it can fail horribly if someone makes a domain like "info.org".
  // Still, I think this is the best we can do short of consulting DNS.

  // Convert from NN_NN_NN_NN back to NN.NN.NN.NN
  $uri = preg_replace("/([0-9]+)_([0-9]+)_([0-9]+)_([0-9]+)/", "$1.$2.$3.$4", $site_dir);

  return 'http://' . $uri;
}