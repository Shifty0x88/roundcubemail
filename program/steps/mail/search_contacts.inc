<?php

/*
 +-----------------------------------------------------------------------+
 | program/steps/mail/search_contacts.inc                                |
 |                                                                       |
 | This file is part of the Roundcube Webmail client                     |
 | Copyright (C) 2013, The Roundcube Dev Team                            |
 |                                                                       |
 | Licensed under the GNU General Public License version 3 or            |
 | any later version with exceptions for skins & plugins.                |
 | See the README file for a full license statement.                     |
 |                                                                       |
 | PURPOSE:                                                              |
 |   Search contacts from the adress book widget                         |
 |                                                                       |
 +-----------------------------------------------------------------------+
 | Author: Thomas Bruederli <roundcube@gmail.com>                        |
 +-----------------------------------------------------------------------+
*/

$search  = get_input_value('_q', RCUBE_INPUT_GPC, true);
$sources = $RCMAIL->get_address_sources();
$search_mode = (int) $RCMAIL->config->get('addressbook_search_mode');
$sort_col = $RCMAIL->config->get('addressbook_sort_col', 'name');
$afields = $RCMAIL->config->get('contactlist_fields');

$page = 1;
$page_size = $RCMAIL->config->get('addressbook_pagesize', $RCMAIL->config->get('pagesize', 50));

$records = $search_set = array();
foreach ($sources as $s) {
    $source = $RCMAIL->get_address_book($s['id']);
    $source->set_page(1);
    $source->set_pagesize(9999);

    // get contacts count
    $result = $source->search($afields, $search, $search_mode, true, true, 'email');

    if (!$result->count) {
        continue;
    }

    // get records
    $result = $source->list_records($afields);

    while ($row = $result->next()) {
        $row['sourceid'] = $s['id'];
        $key = rcube_addressbook::compose_contact_key($row, $sort_col);
        $records[$key] = $row;
    }

    $search_set[$s['id']] = $source->get_search_set();
    unset($result);
}

// sort the records
ksort($records, SORT_LOCALE_STRING);

// create resultset object
$count  = count($records);
$result = new rcube_result_set($count);

// select the requested page
if ($page_size < $count) {
    $records = array_slice($records, $result->first, $page_size);
}

$result->records = array_values($records);

if (!empty($result) && $result->count > 0) {
    // create javascript list
    while ($row = $result->next()) {
        $name = rcube_addressbook::compose_list_name($row);

        // add record for every email address of the contact
        // (same as in list_contacts.inc)
        $emails = $source->get_col_values('email', $row, true);
        foreach ($emails as $i => $email) {
            $row_id = $row['ID'].$i;
            $jsresult[$row_id] = format_email_recipient($email, $name);
            $OUTPUT->command('add_contact_row', $row_id, array(
                'contact' => html::span(array('title' => $email), Q($name ? $name : $email) .
                    ($name && count($emails) > 1 ? '&nbsp;' . html::span('email', Q($email)) : '')
                )), 'person');
        }
    }

    // search request ID
    $search_request = md5('composeaddr' . $search);

    // save search settings in session
    $_SESSION['search'][$search_request] = $search_set;
    $_SESSION['search_params'] = array('id' => $search_request, 'data' => array($afields, $search));

    $OUTPUT->show_message('contactsearchsuccessful', 'confirmation', array('nr' => $result->count));

    $OUTPUT->command('set_env', 'search_request', $search_request);
    $OUTPUT->command('set_env', 'source', '');
    $OUTPUT->command('unselect_directory');
}
else {
    $OUTPUT->show_message('nocontactsfound', 'notice');
}

// update env
$OUTPUT->set_env('contactdata', $jsresult);
$OUTPUT->set_env('pagecount', ceil($result->count / $page_size));
$OUTPUT->command('set_page_buttons');

// send response
$OUTPUT->send();
