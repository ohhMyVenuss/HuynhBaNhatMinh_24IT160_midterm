/*
 * Midterm Project: Implementation of UNIX ls(1)
 *
 * util.c - Implementation of utility functions for memory management,
 *          path handling, mode formatting, time formatting, and classification.
 */

#include "ls.h"
#include "util.h"

/* Six months in seconds: 6 * 30 * 24 * 60 * 60 */
#define SIX_MONTHS_SECS (180L * 24L * 60L * 60L)

/*
 * Allocate memory safely or abort if allocation fails.
 */
void *
xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL && size > 0) {
        perror("ls: malloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

/*
 * Reallocate memory safely or abort if reallocation fails.
 */
void *
xrealloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        perror("ls: realloc");
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

/*
 * Duplicate string safely or abort if memory allocation fails.
 */
char *
xstrdup(const char *str)
{
    if (str == NULL) {
        return NULL;
    }
    size_t len = strlen(str);
    char *copy = (char *)xmalloc(len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}

/*
 * Combine a directory path and a filename into a single path string.
 */
char *
path_join(const char *dir, const char *file)
{
    if (dir == NULL || dir[0] == '\0') {
        return xstrdup(file);
    }
    if (file == NULL || file[0] == '\0') {
        return xstrdup(dir);
    }

    size_t dlen = strlen(dir);
    size_t flen = strlen(file);
    int needs_slash = (dir[dlen - 1] != '/');

    char *result = (char *)xmalloc(dlen + needs_slash + flen + 1);
    memcpy(result, dir, dlen);
    if (needs_slash) {
        result[dlen] = '/';
        memcpy(result + dlen + 1, file, flen + 1);
    } else {
        memcpy(result + dlen, file, flen + 1);
    }
    return result;
}

/*
 * Initialize block size according to -k, -h, or the BLOCKSIZE environment variable.
 */
void
init_blocksize(options_t *opts)
{
    if (opts->opt_k) {
        opts->blocksize = 1024;
        return;
    }
    if (opts->opt_h) {
        /* When -h is used, humanize_number formats bytes directly */
        opts->blocksize = 512;
        return;
    }

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    int headerlen = 0;
    long bsize = 512;
    (void)getbsize(&headerlen, &bsize);
    if (bsize <= 0) {
        bsize = 512;
    }
    opts->blocksize = bsize;
#else
    const char *env = getenv("BLOCKSIZE");
    if (env != NULL && *env != '\0') {
        char *endptr = NULL;
        long val = strtol(env, &endptr, 10);
        if (*endptr == 'k' || *endptr == 'K') {
            val *= 1024;
            endptr++;
        } else if (*endptr == 'm' || *endptr == 'M') {
            val *= 1024 * 1024;
            endptr++;
        } else if (*endptr == 'g' || *endptr == 'G') {
            val *= 1024 * 1024 * 1024;
            endptr++;
        }
        if (val >= 512 && *endptr == '\0') {
            opts->blocksize = val;
            return;
        }
    }
    opts->blocksize = 512;
#endif
}

/*
 * Convert 512-byte blocks (from stat.st_blocks) into units of opts->blocksize,
 * rounding up partial units to the next integer value.
 */
int64_t
compute_blocks(int64_t st_blocks, long blocksize)
{
    if (st_blocks <= 0) {
        return 0;
    }
    if (blocksize <= 0 || blocksize == 512) {
        return st_blocks;
    }
    /* Total bytes consumed by blocks */
    int64_t bytes = st_blocks * 512;
    return (bytes + blocksize - 1) / blocksize;
}

/*
 * Format file mode into standard 10-character string (e.g. "drwxr-xr-x").
 * Complies with NetBSD specification including SUID/SGID ('s'/'S'),
 * sticky bit ('t'/'T'), whiteouts, FIFOs, devices, and sockets.
 */
void
format_mode_string(mode_t mode, char *buf)
{
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    char raw[12];
    strmode(mode, raw);
    /* strmode provides 10 chars + space/alternate access + nul */
    memcpy(buf, raw, 10);
    buf[10] = '\0';
#else
    /* Portable implementation matching NetBSD strmode */
    if (S_ISDIR(mode)) {
        buf[0] = 'd';
    } else if (S_ISCHR(mode)) {
        buf[0] = 'c';
    } else if (S_ISBLK(mode)) {
        buf[0] = 'b';
    } else if (S_ISLNK(mode)) {
        buf[0] = 'l';
    } else if (S_ISSOCK(mode)) {
        buf[0] = 's';
    } else if (S_ISFIFO(mode)) {
        buf[0] = 'p';
#ifdef S_ISWHT
    } else if (S_ISWHT(mode)) {
        buf[0] = 'w';
#endif
    } else {
        buf[0] = '-';
    }

    /* Owner permissions */
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID) {
        buf[3] = (mode & S_IXUSR) ? 's' : 'S';
    } else {
        buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    }

    /* Group permissions */
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID) {
        buf[6] = (mode & S_IXGRP) ? 's' : 'S';
    } else {
        buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    }

    /* Other permissions */
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX) {
        buf[9] = (mode & S_IXOTH) ? 't' : 'T';
    } else {
        buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    }

    buf[10] = '\0';
#endif
}

/*
 * Format date and time according to BSD/POSIX standards:
 * - If within 6 months in the past and not in the future (> 1 hour ahead):
 *     "Mmm dd HH:MM" (e.g. "Oct  8 06:38")
 * - Otherwise (older than 6 months or in future):
 *     "Mmm dd  YYYY" (e.g. "Dec 16  2024")
 */
void
format_entry_time(const file_entry_t *entry, const options_t *opts, char *buf, size_t size)
{
    time_t file_time;
    if (opts->time_type == TIME_ACCESS) {
        file_time = entry->sb.st_atime;
    } else if (opts->time_type == TIME_STATUS_CHANGE) {
        file_time = entry->sb.st_ctime;
    } else {
        file_time = entry->sb.st_mtime;
    }

    struct tm tm_buf;
    struct tm *lt = localtime_r(&file_time, &tm_buf);
    if (lt == NULL) {
        snprintf(buf, size, "------------");
        return;
    }

    time_t now = time(NULL);
    if (file_time > now + 3600 || file_time < now - SIX_MONTHS_SECS) {
        /* Older than 6 months or future date: display year */
        strftime(buf, size, "%b %e  %Y", lt);
    } else {
        /* Recent date: display hour:minute */
        strftime(buf, size, "%b %e %H:%M", lt);
    }
}

/*
 * Format a number into human-readable representation (-h).
 * Matches NetBSD humanize_number output (e.g. 512B, 36K, 9.8M, 10M).
 */
void
format_human_number(char *buf, size_t len, int64_t num)
{
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    char szbuf[5];
    if (humanize_number(szbuf, sizeof(szbuf), num, "", HN_AUTOSCALE,
                        HN_DECIMAL | HN_B | HN_NOSPACE) == -1) {
        snprintf(buf, len, "%" PRId64 "B", num);
    } else {
        snprintf(buf, len, "%s", szbuf);
    }
#else
    /* Portable fallback */
    const char *suffixes[] = {"B", "K", "M", "G", "T", "P", "E"};
    double dval = (double)num;
    size_t sidx = 0;

    while (dval >= 1024.0 && sidx < sizeof(suffixes)/sizeof(suffixes[0]) - 1) {
        dval /= 1024.0;
        sidx++;
    }

    if (sidx == 0) {
        snprintf(buf, len, "%" PRId64 "%s", num, suffixes[sidx]);
    } else if (dval < 10.0) {
        snprintf(buf, len, "%.1f%s", dval, suffixes[sidx]);
    } else {
        snprintf(buf, len, "%.0f%s", dval, suffixes[sidx]);
    }
#endif
}

/*
 * Return the classification character for -F option:
 * '/' for directory
 * '*' for executable
 * '@' for symbolic link
 * '%' for whiteout
 * '=' for socket
 * '|' for FIFO
 * '\0' for regular non-executable file
 */
char
get_classification_char(mode_t mode)
{
    if (S_ISDIR(mode)) {
        return '/';
    }
    if (S_ISLNK(mode)) {
        return '@';
    }
    if (S_ISSOCK(mode)) {
        return '=';
    }
    if (S_ISFIFO(mode)) {
        return '|';
    }
#ifdef S_ISWHT
    if (S_ISWHT(mode)) {
        return '%';
    }
#endif
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        return '*';
    }
    return '\0';
}

/*
 * Output filename handling non-printable characters:
 * - If opts->opt_q is active: replace non-printable characters with '?'
 * - If opts->opt_w is active: output characters verbatim
 */
void
print_safe_filename(const char *name, const options_t *opts)
{
    if (name == NULL) {
        return;
    }
    for (const char *p = name; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if (opts->opt_q && !isprint(c)) {
            putchar('?');
        } else {
            putchar(c);
        }
    }
}
