/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * sort.c - Implementation of entry sorting: lexicographical, time (-t),
 *          size (-S), reverse (-r), and unsorted (-f).
 */

#include "ls.h"
#include "sort.h"

/* Pointer to active options during qsort comparison */
static const options_t *s_sort_opts = NULL;

/*
 * Extract the relevant timestamp (seconds and nanoseconds) from stat.
 */
static void
get_entry_timespec(const file_entry_t *entry, time_type_t ttype, time_t *sec, long *nsec)
{
    if (!entry->stat_ok) {
        *sec = 0;
        *nsec = 0;
        return;
    }

    switch (ttype) {
    case TIME_ACCESS:
        *sec = entry->sb.st_atime;
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
        *nsec = entry->sb.st_atimensec;
#elif defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200809L)
        *nsec = entry->sb.st_atim.tv_nsec;
#else
        *nsec = 0;
#endif
        break;

    case TIME_STATUS_CHANGE:
        *sec = entry->sb.st_ctime;
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
        *nsec = entry->sb.st_ctimensec;
#elif defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200809L)
        *nsec = entry->sb.st_ctim.tv_nsec;
#else
        *nsec = 0;
#endif
        break;

    case TIME_MODIFICATION:
    default:
        *sec = entry->sb.st_mtime;
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
        *nsec = entry->sb.st_mtimensec;
#elif defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200809L)
        *nsec = entry->sb.st_mtim.tv_nsec;
#else
        *nsec = 0;
#endif
        break;
    }
}

/*
 * Compare two entries for qsort:
 * Returns < 0, 0, or > 0.
 */
static int
compare_entries_internal(const void *p1, const void *p2)
{
    const file_entry_t *a = (const file_entry_t *)p1;
    const file_entry_t *b = (const file_entry_t *)p2;
    int res = 0;

    if (s_sort_opts->sort_type == SORT_TIME) {
        time_t a_sec, b_sec;
        long a_nsec, b_nsec;
        get_entry_timespec(a, s_sort_opts->time_type, &a_sec, &a_nsec);
        get_entry_timespec(b, s_sort_opts->time_type, &b_sec, &b_nsec);

        /* Newest first: larger timestamp comes earlier */
        if (a_sec != b_sec) {
            res = (a_sec > b_sec) ? -1 : 1;
        } else if (a_nsec != b_nsec) {
            res = (a_nsec > b_nsec) ? -1 : 1;
        } else {
            /* Tie-breaker: lexicographical order */
            res = strcmp(a->name, b->name);
        }
    } else if (s_sort_opts->sort_type == SORT_SIZE) {
        off_t a_size = a->stat_ok ? a->sb.st_size : 0;
        off_t b_size = b->stat_ok ? b->sb.st_size : 0;

        /* Largest first */
        if (a_size != b_size) {
            res = (a_size > b_size) ? -1 : 1;
        } else {
            /* Tie-breaker: lexicographical order */
            res = strcmp(a->name, b->name);
        }
    } else {
        /* Default: lexicographical order by entry name */
        res = strcmp(a->name, b->name);
    }

    /* Reverse order if -r option is specified */
    if (s_sort_opts->opt_r) {
        res = -res;
    }

    return res;
}

/*
 * Sort entry list in place according to specified options.
 */
void
sort_entry_list(entry_list_t *list, const options_t *opts)
{
    if (list == NULL || list->count <= 1) {
        return;
    }
    if (opts->sort_type == SORT_NONE) {
        /* -f: output is not sorted */
        return;
    }

    s_sort_opts = opts;
    qsort(list->entries, list->count, sizeof(file_entry_t), compare_entries_internal);
    s_sort_opts = NULL;
}
