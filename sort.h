/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * sort.h - Declarations for directory and file entry sorting routines.
 */

#ifndef SORT_H
#define SORT_H

#include "ls.h"

void sort_entry_list(entry_list_t *list, const options_t *opts);

#endif /* SORT_H */
