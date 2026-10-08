# Midterm Project: Implementation of UNIX ls(1)

**Course:** Advanced Programming in the UNIX Environment / System Programming  
**Author:** [Your Full Name]  
**Student ID:** [Your Student ID]  
**GitHub Repository:** [https://github.com/YourUsername/Name_studentID_midterm](https://github.com/YourUsername/Name_studentID_midterm)  
**Target Environment:** NetBSD 10.1 / POSIX-compliant UNIX systems  

---

## 1. Project Overview

This project is a modular, robust, from-scratch implementation of the UNIX `ls(1)` command based on the **NetBSD 10.1 General Commands Manual** specification provided.

The utility lists directory contents and file metadata using UNIX system calls (`stat(2)`, `lstat(2)`, `opendir(3)`, `readdir(3)`, `closedir(3)`, `readlink(2)`). It adheres strictly to UNIX filesystem conventions, modular design principles, defensive programming, and memory safety.

---

## 2. Architecture & Modular Decomposition

The codebase is organized into modular source and header files, separating concerns between CLI argument parsing, directory traversal, sorting algorithms, formatting/display, and helper utilities.

```
├── ls.h           # Core header defining data structures, enums, and prototypes
├── main.c         # Entry point, flag parsing (getopt), override handling, operand routing
├── traverse.h     # Declarations for directory reading and recursion
├── traverse.c     # Directory entry reading, filtering (-a, -A, -f), and recursion (-R)
├── sort.h         # Declarations for sorting routines
├── sort.c         # Sorting algorithms: name, time (-t), size (-S), reverse (-r), unsorted (-f)
├── display.h      # Declarations for formatting and output rendering
├── display.c      # Dynamic column alignment, long format (-l, -n), inode (-i), blocks (-s), -F
├── util.h         # Utility declarations
├── util.c         # Memory wrappers, path helpers, strmode formatting, humanize_number, block sizing
├── Makefile       # Compilation, testing, and cleanup rules
├── test_suite.sh  # Automated test suite covering 45+ verification checks
└── .gitignore     # Git configuration ignoring object files and compiled binaries
```

### Module Responsibilities

1. **`main.c` (Driver & Argument Processing)**:
   - Parses flags using `getopt(3)` with `AacdFfhiklnqRrSstuw`.
   - Resolves flag conflicts according to manual precedence rules.
   - Determines TTY status via `isatty(3)` to select default non-printable behavior (`-q` vs `-w`).
   - Checks superuser privileges (`geteuid() == 0`) to default to `-A` when appropriate.
   - Partitions command-line operands into file operands and directory operands, ensuring non-directory operands are displayed first.
   - Manages process exit status (`0` on success, `> 0` if any operand failed).

2. **`traverse.c` & `traverse.h` (Filesystem Traversal)**:
   - Reads directory entries using `opendir(3)` and `readdir(3)`.
   - Filters entries based on dot-file policies (`-a`, `-A`, default hiding).
   - Performs `lstat(2)` on each entry to prevent unintentional symlink dereferencing.
   - Executes recursive traversal (`-R`) across subdirectories (skipping `.` and `..` and symlinks to prevent cycles).

3. **`sort.c` & `sort.h` (Sorting Engine)**:
   - Implements `qsort(3)`-compatible comparators:
     - **Lexicographical** (`strcmp`) by default.
     - **Time** (`-t`): modification time, access time (`-u`), or status change time (`-c`), with nanosecond precision tie-breaking, falling back to lexicographical order.
     - **Size** (`-S`): file size in descending order, tie-breaking lexicographically.
     - **Reverse** (`-r`): reverses any active ordering.
     - **Unsorted** (`-f`): preserves readdir order without sorting.

4. **`display.c` & `display.h` (Output Rendering)**:
   - Computes dynamic column widths per batch to ensure aligned fields.
   - Renders default single-column listing (one entry per line).
   - Renders long format (`-l`, `-n`):
     - Mode string (10 characters, handling SUID, SGID, sticky bit, special file types).
     - Link count.
     - Owner name or numeric UID (`-n`).
     - Group name or numeric GID (`-n`).
     - File size or device numbers (`major, minor` for char/block special files).
     - Human-readable byte formatting (`-h`).
     - Modification date/time (recent vs >6 months/future).
     - Symbolic link targets (`-> target`).
   - Renders inode serial numbers (`-i`) and block counts (`-s`).
   - Computes and prints directory `total` block line.

5. **`util.c` & `util.h` (System Utilities)**:
   - Safe memory allocation wrappers (`xmalloc`, `xrealloc`, `xstrdup`).
   - Path manipulation (`path_join`).
   - `format_mode_string`: uses `strmode(3)` on BSD/NetBSD, with portable fallback.
   - `format_human_number`: uses `humanize_number(3)` with `HN_AUTOSCALE | HN_B | HN_NOSPACE | HN_DECIMAL`.
   - `init_blocksize` and `compute_blocks`: calculates block counts based on 512-byte blocks, `-k` (1024), `-h`, or `BLOCKSIZE` environment variable.
   - `print_safe_filename`: escapes non-printable characters with `?` under `-q` or prints raw characters under `-w`.

---

## 3. Implemented Options Reference

All 19 options from `ls [-AacdFfhiklnqRrSstuw] [file ...]` are fully supported:

| Option | Description | Implementation Details |
| :--- | :--- | :--- |
| `-A` | List all entries except `.` and `..` | Automatically active for superuser (`geteuid() == 0`). Overridden if `-a` is also given. |
| `-a` | Include entries starting with `.` | Displays hidden files as well as `.` and `..`. |
| `-c` | Use file status change time (`st_ctime`) | Modifies sorting (`-t`) and long format date display (`-l`). Overrides `-u`. |
| `-d` | Treat directories as plain files | Does not recurse into directories or indirect through symlink operands. Overrides `-R`. |
| `-F` | Display classification indicator | Appends `/` (dir), `*` (executable), `@` (symlink), `=` (socket), `\|` (FIFO), `%` (whiteout). |
| `-f` | Output is not sorted | Disables sorting and forces `-a` to include all directory entries in disk order. |
| `-h` | Human-readable sizes | Scales sizes in bytes (e.g. `512B`, `36K`, `9.8M`, `10M`) for `-l` and `-s`. Overrides `-k`. |
| `-i` | Print file serial number (inode) | Displays `st_ino` before other file details. |
| `-k` | Sizes reported in kilobytes | Displays block counts in 1024-byte units for `-s`. Rightmost of `-k` and `-h` wins. |
| `-l` | List in long format | Displays mode, link count, owner, group, size/devices, date/time, filename, and symlink target. |
| `-n` | Numeric IDs in long format | Identical to `-l` except owner and group IDs are displayed numerically. Overrides `-l`. |
| `-q` | Replace non-printable characters with `?` | Default when stdout is a terminal. Overrides `-w`. |
| `-R` | Recursively list subdirectories | Recursively enters subdirectories. Prevents recursion into `.` and `..` and symlinks. Overrides `-d`. |
| `-r` | Reverse sort order | Inverts the active sort criterion (alphabetical, time, or size). |
| `-S` | Sort by file size | Largest files first. Tie-break: lexicographical order. Overrides `-t`. |
| `-s` | Display file system block count | Shows number of allocated blocks (adjusted for 512, 1024, or `BLOCKSIZE`). Shows `total` for dirs. |
| `-t` | Sort by timestamp | Newest files first. Uses mtime, atime (`-u`), or ctime (`-c`). Nanosecond precision. Overrides `-S`. |
| `-u` | Use last access time (`st_atime`) | Modifies sorting (`-t`) and long format date display (`-l`). Overrides `-c`. |
| `-w` | Force raw printing of non-printable chars | Default when stdout is NOT a terminal. Overrides `-q`. |

---

## 4. Precedence & Conflict Resolution Rules

The manual specifies several override relationships:
1. **`-w` vs `-q`**: The last one specified on the command line determines whether non-printable characters are printed raw or replaced with `?`. Default is `-q` if output is a TTY, else `-w`.
2. **`-l` vs `-n`**: The last one specified determines whether owner and group are displayed by name or numerical ID.
3. **`-c` vs `-u`**: The last one specified determines the timestamp used (`st_ctime` vs `st_atime`). If neither is specified, `st_mtime` is used.
4. **`-R` vs `-d`**: The last one specified determines whether directories are recursively traversed or listed as plain files.
5. **`-k` vs `-h`**: The rightmost flag determines whether blocks/sizes are scaled to kilobytes or human-readable format.
6. **`-t` vs `-S`**: The last sort flag determines whether sorting is by timestamp or by file size.
7. **`-f`**: Forces unsorted output and implies `-a`, overriding any sort criteria.

---

## 5. Edge Cases & Robustness

- **Non-existent Operands**: When given valid and invalid paths (e.g. `./ls /bin /does_not_exist`), an error message is printed to stderr for the invalid path, the valid operand is still processed and displayed, and the process exits with `1`.
- **Symbolic Links**:
  - For operands: if `-d`, `-l`, or `-F` is passed without a trailing slash, `lstat` is used so the symlink itself is reported. Otherwise, `stat` is used so directory symlinks are traversed.
  - Broken symlinks are handled gracefully without segmentation faults.
  - In long format, targets are resolved via `readlink` and printed as `name -> target`.
- **Special Device Files**: Detects `S_ISCHR` and `S_ISBLK` and formats device nodes as `major, minor` rather than regular file size, aligning columns cleanly.
- **Permission Denied**: If a subdirectory cannot be opened (`opendir` returns `NULL`), an error message is logged to stderr, exit status is updated to `1`, and traversal continues for remaining paths.
- **Cycle Prevention**: In recursive traversal (`-R`), `.` and `..` are explicitly ignored, and symlinks to directories are not recursed into.
- **Clean Memory Management**: All allocated paths, directory entry arrays, and string duplicates are freed before function return.

---

## 6. How to Build & Run

### Compilation
The provided `Makefile` supports both BSD `make` (NetBSD) and GNU `make` (Linux) with strict warning flags (`-Wall -Wextra -Werror -pedantic -std=c99`):

```sh
# Compile the executable 'ls'
make

# Clean up binaries and object files
make clean
```

### Running the Program
```sh
# Basic usage
./ls

# Long format with human-readable sizes
./ls -lh /bin

# Recursive listing of subdirectories
./ls -R dir_name

# Sort by size in reverse order
./ls -lSr

# Long format with numeric IDs and inode numbers
./ls -lni
```

### Automated Testing
The repository includes a comprehensive automated test script `test_suite.sh` that validates all 45 test scenarios:

```sh
# Run automated test suite via Makefile
make test

# Or run directly
chmod +x test_suite.sh
./test_suite.sh
```

---

## 7. Step-by-Step Verification Guide

To test each feature individually, run the following test commands:

### 1. Basic Listing & Operand Separation
```sh
# Test 1: No arguments (current directory)
./ls

# Test 2: File operands displayed before directory operands
./ls /bin/cat /bin
# Output: /bin/cat is displayed first, followed by /bin: header and its contents
```

### 2. Error Handling & Exit Codes
```sh
# Test 3: Non-existent file
./ls /nonexistent_file
echo "Exit code: $?"
# Expected: prints 'ls: /nonexistent_file: No such file or directory' to stderr, exit code 1

# Test 4: Mixed valid and invalid operands
./ls /bin/cat /nonexistent_file
echo "Exit code: $?"
# Expected: error on stderr for nonexistent, displays /bin/cat, exit code 1
```

### 3. Hidden File Filtering (-a, -A)
```sh
# Test 5: Default (hides hidden files)
./ls -d . .*
./ls

# Test 6: -a (shows all files including . and ..)
./ls -a

# Test 7: -A (shows hidden files but excludes . and ..)
./ls -A
```

### 4. Long Format & Numeric IDs (-l, -n)
```sh
# Test 8: Long listing with names
./ls -l /bin/cat
# Output: -r-xr-xr-x  1 root  wheel  20352 Dec 16  2024 /bin/cat

# Test 9: Long listing with numeric UID and GID
./ls -n /bin/cat
# Output: -r-xr-xr-x  1 0  0  20352 Dec 16  2024 /bin/cat

# Test 10: Precedence override (-ln then -nl)
./ls -ln /bin/cat   # Numeric format
./ls -nl /bin/cat   # Named format
```

### 5. Block Size & Human-Readable (-s, -h, -k, BLOCKSIZE)
```sh
# Test 11: Block count (-s)
./ls -s /bin/cat

# Test 12: Human-readable sizes (-h)
./ls -lh /bin/cat
# Output: -r-xr-xr-x  1 root  wheel   20K Dec 16  2024 /bin/cat

# Test 13: Kilobyte units (-k)
./ls -sk /bin/cat

# Test 14: BLOCKSIZE environment variable
BLOCKSIZE=1024 ./ls -s /bin/cat
```

### 6. Inodes & Classification Symbols (-i, -F)
```sh
# Test 15: Inode numbers (-i)
./ls -i /bin/cat

# Test 16: Classification indicators (-F)
./ls -F /bin
# Output: directories end with '/', executables end with '*', symlinks end with '@'
```

### 7. Directory as Plain Files (-d) & Recursive (-R)
```sh
# Test 17: List directory as file (-d)
./ls -ld /bin
# Output: drwxr-xr-x  2 root  wheel  1024 Dec 16  2024 /bin

# Test 18: Recursive listing (-R)
./ls -R /home/jschauma/apue-code/05
```

### 8. Sorting Criteria (-t, -u, -c, -S, -r, -f)
```sh
# Test 19: Sort by modification time (-t)
./ls -lt /etc

# Test 20: Sort by access time (-tu)
./ls -ltu /etc

# Test 21: Sort by status change time (-tc)
./ls -ltc /etc

# Test 22: Sort by file size (-S)
./ls -lS /bin

# Test 23: Reverse order (-r)
./ls -lSr /bin

# Test 24: Unsorted directory order (-f)
./ls -f /etc
```

### 9. Special Files & Symlinks
```sh
# Test 25: Device special files in /dev (major, minor)
./ls -l /dev/MAKEDEV /dev/acpi /dev/altmem0
# Output: formats crw-... and brw-... with major, minor numbers

# Test 26: Symlink with target display
./ls -l /bin/chgrp
# Output: lrwxr-xr-x  1 root  wheel  11 Dec 16  2024 /bin/chgrp -> /sbin/chown
```

---

## 8. GitHub Repository & Submission Instructions

### Creating and Pushing to GitHub
1. In your local workspace, initialize Git and commit the project files:
   ```sh
   git init
   git add Makefile README.md .gitignore ls.h util.h util.c sort.h sort.c display.h display.c traverse.h traverse.c main.c test_suite.sh
   git commit -m "Implement UNIX ls(1) with NetBSD 10.1 specifications"
   ```
2. Create a new repository on GitHub named:
   `Name_studentID_midterm`
3. Add the remote and push:
   ```sh
   git remote add origin https://github.com/<YourUsername>/Name_studentID_midterm.git
   git branch -M main
   git push -u origin main
   ```
4. Verify that no compiled binaries (`ls`) or object files (`*.o`) were committed (verified via `.gitignore`).
5. Update the repository link in `README.md` and submit your report to the e-learning portal.
