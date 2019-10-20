<?php

/**
 * Implementation of hook_drop_command().
 *
 * In this hook, you specify which commands your
 * drop module makes available, what it does and
 * description.
 *
 * Notice how this structure closely resembles how
 * you define menu hooks.
 *
 * @return
 *   An associative array describing your command(s).
 */
function variable_drop_command() {
  $items['variable-get'] = array(
    'description' => 'Get a list of some or all site variables and values.',
    'arguments' => array(
      'name' => 'A string to filter the variables by.  Only variables beginning with the string will be listed.',
    ),
    'examples' => array(
      'drop vget' => 'List all variables and values.',
      'drop vget user' => 'List all variables beginning with the string "user".',
    ),
    'options' => array(
      'pipe' => 'Use var_export() to emit executable PHP. Useful for pasting into code.',
    ),
    'aliases' => array('vget'),
  );
  $items['variable-set'] = array(
    'description' => "Set a variable.",
    'arguments' => array(
      'name' => 'The name of a variable or the first few letters of its name.',
      'value' => 'The value to assign to the variable.',
    ),
    'options' => array(
      'yes' => 'Skip confirmation if only one variable name matches.',
      'always-set' => 'Always skip confirmation.',
    ),
    'examples' => array(
      'drop vset --yes preprocess_css 1' => 'Set the preprocess_css variable to true. Skip confirmation if variable already exists.',
      'drop vset --always-set site_offline 1' => 'Take the site offline; Skips confirmation even if site_offline variable does not exist.',
      'drop vset pr 1' => 'Choose from a list of variables beginning with "pr" to set to true.',
    ),
    'aliases' => array('vset'),
  );
  $items['variable-delete'] = array(
    'description' => "Delete a variable.",
    'arguments' => array(
      'name' => 'The name of a variable or the first few letters of its name.',
    ),
    'options' => array(
      'yes' => 'Skip confirmation if only one variable name matches.',
    ),
    'examples' => array(
      'drop vdel user_pictures' => 'Delete the user_pictures variable.',
      'drop vdel u' => 'Choose from a list of variables beginning with "u" to delete.',
    ),
    'aliases' => array('vdel'),
  );

  return $items;
}

/**
 * Command callback.
 * List your site's variables.
 */
function drop_variable_get() {
  global $conf;
  $found = FALSE;
  
  $keys = array_keys($conf);
  if ($args = func_get_args()) {
    $keys = preg_grep("/{$args[0]}/", $keys);
  }
  foreach ($keys as $name) {
    $value = $conf[$name];
    drop_print_pipe("\$variables['$name'] = ". var_export($value, TRUE). ";\n");
    $returns[$name] = $value;

    if ($value === TRUE) {
      $value = 'TRUE';
    }
    elseif ($value === FALSE) {
      $value = 'FALSE';
    }
    elseif (is_string($value)) {
      $value = '"' . $value . '"';
    }
    elseif (is_array($value) || is_object($value)) {
      $value = print_r($value, TRUE);
    }
    drop_print($name . ': ' . $value);
    $found = TRUE;
  }
  
  if (!$found) {
    return drop_set_error('No matching variable found.');
  }
  else {
    return $returns;
  }
}

/**
 * Command callback.
 * Set a variable.
 */
function drop_variable_set() {
  $args = func_get_args();
  if (!isset($args[0])) {
    return drop_set_error('DROP_VARIABLE_ERROR', dt('No variable specified.'));
  }
  $value = $args[1];
  if (!isset($value)) {
    return drop_set_error('DROP_VARIABLE_ERROR', dt('No value specified.'));
  }

  $result = drop_variable_like($args[0]);

  $options[] = "$args[0] ". dt('(new variable)');
  $match = FALSE;
  while (!$match && $name = drop_db_result($result)) {
    if ($name == $args[0]) {
      $options[0] = $name;
      $match = TRUE;
    }
    else {
      $options[] = $name;
    }
  }

  if ((drop_get_option('always-set', FALSE)) || ((count($options) == 1 || $match) && drop_get_context('DROP_AFFIRMATIVE'))) {
    variable_set($args[0], $value);
    drop_log(dt('!name was set to !value.', array('!name' => $args[0], '!value' => $value)), 'success');
    return '';
  }
  else {
    $choice = drop_choice($options, 'Enter a number to choose which variable to set.');
    if ($choice !== FALSE) {
      $choice = $options[$choice];
      $choice = str_replace(' ' . dt('(new variable)'), '', $choice);
      drop_op('variable_set', $choice, $value);
      drop_log(dt('!name was set to !value', array('!name' => $choice, '!value' => $value)), 'success');
    }
  }
}

/**
 * Command callback.
 * Delete a variable.
 */
function drop_variable_delete() {
  $args = func_get_args();
  if (!isset($args[0])) {
    drop_set_error('DROP_VARIABLE_ERROR', dt('No variable specified'));
  }
  // Look for similar variable names.
  $result = drop_variable_like($args[0]);

  $options = array();
  while ($name = drop_db_result($result)) {
    $options[] = $name;
  }

  if (count($options) == 0) {
    drop_print(dt('!name not found.', array('!name' => $args[0])));
    return '';
  }

  if ((count($options) == 1) && drop_get_context('DROP_AFFIRMATIVE')) {
    drop_op('variable_del', $args[0]);
    drop_log(dt('!name was deleted.', array('!name' => $args[0])), 'success');
    return '';
  }
  else {
    $choice = drop_choice($options, 'Enter a number to choose which variable to delete.');
    if ($choice !== FALSE) {
      $choice = $options[$choice];
      drop_op('variable_del', $choice);
      drop_log(dt('!choice was deleted.', array('!choice' => $choice)), 'success');
    }
  }
}

// Query for similar variable names.
function drop_variable_like($arg) {
  return drop_db_select('variable', 'name', 'name LIKE :keyword', array(':keyword' => $arg . '%'), NULL, NULL, 'name');
}