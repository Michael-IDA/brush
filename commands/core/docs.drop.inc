<?php

/**
 * @file
 *   Documentation commands providing various topics.
 */

/**
 * Implementation of hook_drop_help().
 */
function docs_drop_help($section) {
  switch ($section) {
    case 'meta:docs:title':
      return dt('Documentation commands');
    case 'meta:docs:summary':
      return dt('Show information on various drop topics.');
  }
}

/**
 * Implementation of hook_drop_command().
 *
 * @return
 *   An associative array describing your command(s).
 */
function docs_drop_command() {
  $docs_dir = drop_get_context('DOC_PREFIX', DROP_BASE_PATH);

  //
  // Topic commands.
  // Any commandfile may add topics.
  // Set 'topic' => TRUE to indicate the command is a topic (REQUIRED)
  // Begin the topic name with the name of the commandfile (just like
  // any other command).
  //
  $items['docs-readme'] = array(
    'description' => 'README.txt',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/README.txt'),
  );
  $items['docs-configuration'] = array(
    'description' => 'Drop configuration overview with examples',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/examples/example.droprc.php'),
  );
  $items['docs-aliases'] = array(
    'description' => 'Site aliases overview with examples',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/examples/example.aliases.droprc.php'),
  );
  $items['docs-ini-files'] = array(
    'description' => 'Configuring php.ini or drop.ini for use with drop.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/examples/example.drop.ini'),
  );
  $items['docs-bastion'] = array(
    'description' => 'How to use a bastion server to remotely operate on Backdrop sites behind a firewall.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/docs/bastion.html'),
  );
  $items['docs-bootstrap'] = array(
    'description' => 'Information about the drop bootstrap process.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/docs/bootstrap.html'),
  );
  $items['docs-scripts'] = array(
    'description' => 'Overview on how to write drop shell scripts.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/docs/shellscripts.html'),
  );
  $items['docs-commands'] = array(
    'description' => 'Overview on how to write drop commands.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/docs/commands.html'),
  );
  $items['docs-errorcodes'] = array(
    'description' => 'Summary of drop error codes.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
  );
  $items['docs-api'] = array(
    'description' => 'Drop API',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/docs/drop.api.php'),
  );
  $items['docs-context'] = array(
    'description' => 'Drop contexts',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/docs/context.html'),
  );
  $items['docs-examplescript'] = array(
    'description' => 'Example drop script',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/examples/helloworld.script'),
  );
  $items['docs-examplecommand'] = array(
    'description' => 'Example drop command file.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/examples/sandwich.drop.inc'),
  );
  $items['docs-policy'] = array(
    'description' => 'Example policy file.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/examples/policy.drop.inc'),
  );
  $items['docs-upgrading'] = array(
    'description' => 'Upgrading Backdrop using the drop site-upgrade command.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DROP_BOOTSTRAP_DROP,
    'callback' => 'drop_print_file',
    'callback arguments' => array($docs_dir . '/docs/upgrade.html'),
  );
  return $items;
}

/**
 * docs-error-codes command.  Print a list of all error codes
 * that can be found.
 */
function drop_docs_errorcodes() {
  $header = <<<EOD
==== Drop Error Codes ====

Drop error codes are alphanumeric constants that represent an unrecoverable error condition that may arise during the execution of some command.  They are set by the following function:

  return drop_set_error('DROP_ERROR_CODE', dt('Error message.'));

In general, any drop command that calls drop_set_error is expected to also return FALSE as its function result.  The drop_set_error function returns FALSE to make it easy to exit with an error code.  Error codes are returned as part of the drop backend invoke process, which is used by drop API functions such as drop_invoke_process.  An example of how to test for a specific error code is shown below:

  \$result = drop_invoke_process('some-command');
  if (array_key_exists('DROP_ERROR_CODE', \$result['error_log'])) {
    // handle ocurrances of DROP_ERROR_CODE here
  }

Some of the available drop error codes are listed in the table below.


EOD;

  // Find all of the files that we will search for error messages.
  // Start with all of the commandfiles.
  $commandfiles = drop_commandfile_list();
  $files = array_flip($commandfiles);
  // In addition to the commandfiles, we will also look for files
  // that drop will load when executing a command; examples include
  // updatecode.pm.inc and sync.sql.inc.
  $commands = drop_get_commands();
  foreach ($commands as $command_name => $command) {
    $files = array_merge($files, drop_command_get_includes($command_name));
  }
  // We will also search through all of the .inc files in the
  // drop includes directory
  $drop_include_files = drop_scan_directory(DROP_BASE_PATH . '/includes', '/.*\.inc$/', array('.', '..', 'CVS'), 0, FALSE);
  foreach ($drop_include_files as $filename => $info) {
    $files[$filename] = 'include';
  }

  // Extract error messages from all command files
  $error_list = array();
  foreach ($files as $file => $commandfile) {
    _drop_docs_find_set_error_calls($error_list, $file, $commandfile);
  }
  // Order error messages alphabetically by key
  ksort($error_list);
  // Convert to a table
  $data = array();
  foreach ($error_list as $error_code => $error_messages) {
    $data[] = array($error_code, '-', implode("\n", $error_messages));
  }

  $tmpfile = drop_tempnam('drop-errorcodes.');
  file_put_contents($tmpfile, $header);
  drop_print_table($data, FALSE, array(0 => 35), $tmpfile);
  drop_print_file($tmpfile);
}

/**
 * Search through a php source file looking for calls to
 * the function drop_set_error.  If found, and if the
 * first parameter is an uppercase alphanumeric identifier,
 * then record the error code and the error message in our table.
 */
function _drop_docs_find_set_error_calls(&$error_list, $filename, $shortname) {
  $lines = file($filename);
  foreach ($lines as $line) {
    $matches = array();
    // Find the error code after the drop_set_error call.  The error code
    // should consist of uppercase letters and underscores only (numbers thrown in just in case)
    $match_result = preg_match("/.*drop_set_error[^'\"]['\"]([A-Z0-9_]*)['\"][^,]*,[^'\"]*(['\"])/", $line, $matches);
    if ($match_result) {
      $error_code = $matches[1];
      $quote_char = $matches[2];
      $error_message = "";
      $message_start = strlen($matches[0]) - 1;

      // Regex adapted from http://stackoverflow.com/questions/1824325/regex-expression-for-escaped-quoted-string-wont-work-in-phps-preg-match-allif ($quote_char == '"') {
      if ($quote_char == '"') {
	$regex = '/"((?:[^\\\]*?(?:\\\")?)*?)"/';
      }
      else {
	$regex = "/'((?:[^\\\]*?(?:\\\')?)*?)'/";
      }
      $match_result = preg_match($regex, $line, $matches, 0, $message_start);

      if ($match_result) {
	$error_message = $matches[1];
      }
      $error_list[$error_code] = array_key_exists($error_code, $error_list) ? array_merge($error_list[$error_code], array($error_message)) : array($error_message);
    }
  }
}