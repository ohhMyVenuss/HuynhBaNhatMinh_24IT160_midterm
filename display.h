/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * display.h - Declarations for formatting and displaying file entries.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "ls.h"

void display_entry_list(const entry_list_t *list, const options_t *opts, int is_dir_contents);

#endif /* DISPLAY_H */
