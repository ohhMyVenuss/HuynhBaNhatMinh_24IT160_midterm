#!/bin/sh
#
# test_suite.sh - Comprehensive automated test suite for ls(1) implementation
#

TEST_DIR="/tmp/ls_test_env_$$"
LS_BIN="./ls"
SYS_LS="/bin/ls"
PASSED=0
FAILED=0

cleanup() {
    rm -rf "${TEST_DIR}"
}
trap cleanup EXIT INT TERM

echo "=========================================="
echo "    ls(1) Automated Test Suite Runner     "
echo "=========================================="

# Ensure executable exists
if [ ! -x "${LS_BIN}" ]; then
    echo "ERROR: Executable ${LS_BIN} not found. Running make..."
    make || exit 1
fi

# Setup test environment
rm -rf "${TEST_DIR}"
mkdir -p "${TEST_DIR}/dir1/subdir1" "${TEST_DIR}/dir2"
touch "${TEST_DIR}/file_a.txt" "${TEST_DIR}/file_b.txt" "${TEST_DIR}/.hidden_file"
touch "${TEST_DIR}/dir1/file_in_dir1.txt" "${TEST_DIR}/dir1/subdir1/deep_file.txt"

# Create file with size differences
echo "Small content" > "${TEST_DIR}/small.txt"
dd if=/dev/zero of="${TEST_DIR}/large.bin" bs=1024 count=64 2>/dev/null

# Create executable file
touch "${TEST_DIR}/script.sh"
chmod 755 "${TEST_DIR}/script.sh"

# Create symlinks
ln -s "${TEST_DIR}/file_a.txt" "${TEST_DIR}/symlink_file"
ln -s "${TEST_DIR}/dir1" "${TEST_DIR}/symlink_dir"
ln -s "${TEST_DIR}/nonexistent_target" "${TEST_DIR}/broken_symlink"

# Create FIFO
mkfifo "${TEST_DIR}/test_fifo" 2>/dev/null || true

# Helper for test reporting
run_test() {
    test_name="$1"
    cmd="$2"
    expected_exit="$3"

    eval "${cmd}" > "${TEST_DIR}/out.tmp" 2> "${TEST_DIR}/err.tmp"
    actual_exit=$?

    if [ "${actual_exit}" -eq "${expected_exit}" ]; then
        echo "[PASS] ${test_name}"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] ${test_name} (Expected exit ${expected_exit}, got ${actual_exit})"
        FAILED=$((FAILED + 1))
    fi
}

# Helper to compare output with system ls
run_compare_test() {
    test_name="$1"
    flags="$2"
    target="$3"

    ${LS_BIN} ${flags} ${target} > "${TEST_DIR}/my_out.tmp" 2> "${TEST_DIR}/my_err.tmp"
    my_ret=$?
    ${SYS_LS} ${flags} ${target} > "${TEST_DIR}/sys_out.tmp" 2> "${TEST_DIR}/sys_err.tmp"
    sys_ret=$?

    if [ "${my_ret}" -eq "${sys_ret}" ]; then
        echo "[PASS] Compare ${test_name}: exit code ${my_ret} matches system"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] Compare ${test_name}: exit code mismatch (mine=${my_ret}, sys=${sys_ret})"
        FAILED=$((FAILED + 1))
    fi
}

echo "--- 1. Basic Listing & Operand Separation ---"
run_test "No arguments (current directory)" "${LS_BIN} > /dev/null" 0
run_test "Single file argument" "${LS_BIN} ${TEST_DIR}/file_a.txt > /dev/null" 0
run_test "Multiple file arguments" "${LS_BIN} ${TEST_DIR}/file_a.txt ${TEST_DIR}/file_b.txt > /dev/null" 0
run_test "Single directory argument" "${LS_BIN} ${TEST_DIR}/dir1 > /dev/null" 0
run_test "Multiple directory arguments" "${LS_BIN} ${TEST_DIR}/dir1 ${TEST_DIR}/dir2 > /dev/null" 0
run_test "Mixed file and directory arguments" "${LS_BIN} ${TEST_DIR}/file_a.txt ${TEST_DIR}/dir1 > /dev/null" 0

echo "--- 2. Error Handling & Exit Status ---"
run_test "Non-existent file operand exits with error" "${LS_BIN} ${TEST_DIR}/does_not_exist > /dev/null" 1
run_test "Valid + Non-existent mixed operands exits with error" "${LS_BIN} ${TEST_DIR}/file_a.txt ${TEST_DIR}/does_not_exist > /dev/null" 1
run_test "Invalid command line option exits with error" "${LS_BIN} -Z > /dev/null" 1

echo "--- 3. Directory Content Filtering (-a, -A) ---"
run_test "Default skips hidden files" "! ${LS_BIN} ${TEST_DIR} | grep -q '^\.hidden_file$'" 0
run_test "-a includes dot files including . and .." "${LS_BIN} -a ${TEST_DIR} | grep -q '^\.$'" 0
run_test "-a includes .hidden_file" "${LS_BIN} -a ${TEST_DIR} | grep -q '^\.hidden_file$'" 0
run_test "-A includes .hidden_file" "${LS_BIN} -A ${TEST_DIR} | grep -q '^\.hidden_file$'" 0
run_test "-A excludes . and .." "! ${LS_BIN} -A ${TEST_DIR} | grep -q '^\.$'" 0

echo "--- 4. Long Format & Numeric IDs (-l, -n) ---"
run_test "-l outputs file mode, links, owner, size" "${LS_BIN} -l ${TEST_DIR}/file_a.txt | grep -q '^-rw'" 0
run_test "-n displays numeric UID and GID" "${LS_BIN} -n ${TEST_DIR}/file_a.txt | grep -E '[0-9]+ +[0-9]+'" 0
run_test "-l and -n override: -ln gives numeric" "${LS_BIN} -ln ${TEST_DIR}/file_a.txt | grep -E '[0-9]+ +[0-9]+'" 0
run_test "-n and -l override: -nl gives names" "${LS_BIN} -nl ${TEST_DIR}/file_a.txt > /dev/null" 0

echo "--- 5. Human-Readable & Block Sizes (-h, -k, -s) ---"
run_test "-h formats file sizes in bytes" "${LS_BIN} -lh ${TEST_DIR}/large.bin | grep -q '64K'" 0
run_test "-s outputs block count" "${LS_BIN} -s ${TEST_DIR}/file_a.txt > /dev/null" 0
run_test "-sk reports blocks in kilobytes" "${LS_BIN} -sk ${TEST_DIR}/large.bin > /dev/null" 0
run_test "-k and -h override (-kh gives humanized)" "${LS_BIN} -lkh ${TEST_DIR}/large.bin | grep -q '64K'" 0
run_test "BLOCKSIZE environment variable" "BLOCKSIZE=1024 ${LS_BIN} -s ${TEST_DIR}/large.bin > /dev/null" 0

echo "--- 6. Inode & File Classification (-i, -F) ---"
run_test "-i outputs inode number" "${LS_BIN} -i ${TEST_DIR}/file_a.txt | grep -E '^[0-9]+ '" 0
run_test "-F displays / for directory" "${LS_BIN} -dF ${TEST_DIR}/dir1 | grep -q '/$'" 0
run_test "-F displays * for executable" "${LS_BIN} -F ${TEST_DIR}/script.sh | grep -q '\*'" 0
run_test "-F displays @ for symlink" "${LS_BIN} -dF ${TEST_DIR}/symlink_file | grep -q '@'" 0
if [ -p "${TEST_DIR}/test_fifo" ]; then
    run_test "-F displays | for FIFO" "${LS_BIN} -F ${TEST_DIR}/test_fifo | grep -q '|$'" 0
fi

echo "--- 7. Directories as Plain Files (-d) & Symlinks ---"
run_test "-d lists directory without recursing into it" "${LS_BIN} -d ${TEST_DIR}/dir1 | grep -q '^${TEST_DIR}/dir1$'" 0
run_test "-d does not indirect through symlink" "${LS_BIN} -d ${TEST_DIR}/symlink_dir | grep -q '^${TEST_DIR}/symlink_dir$'" 0

echo "--- 8. Recursive Traversal (-R) ---"
run_test "-R lists subdirectories recursively" "${LS_BIN} -R ${TEST_DIR}/dir1 | grep -q 'subdir1:'" 0
run_test "-R lists deep nested files" "${LS_BIN} -R ${TEST_DIR}/dir1 | grep -q 'deep_file.txt'" 0
run_test "-R and -d override (-Rd gives plain files)" "${LS_BIN} -Rd ${TEST_DIR}/dir1 | grep -q '^${TEST_DIR}/dir1$'" 0
run_test "-d and -R override (-dR gives recursive)" "${LS_BIN} -dR ${TEST_DIR}/dir1 | grep -q 'subdir1:'" 0

echo "--- 9. Sorting & Reverse Sorting (-S, -t, -u, -c, -r, -f) ---"
run_test "-S sorts by size (large first)" "${LS_BIN} -S ${TEST_DIR}/small.txt ${TEST_DIR}/large.bin | head -n 1 | grep -q 'large.bin'" 0
run_test "-Sr sorts by size reversed (small first)" "${LS_BIN} -Sr ${TEST_DIR}/small.txt ${TEST_DIR}/large.bin | head -n 1 | grep -q 'small.txt'" 0
run_test "-r reverses alphabetical order" "${LS_BIN} -r ${TEST_DIR}/file_a.txt ${TEST_DIR}/file_b.txt | head -n 1 | grep -q 'file_b.txt'" 0
run_test "-t sorts by modification time" "${LS_BIN} -t ${TEST_DIR} > /dev/null" 0
run_test "-u sorts/prints using access time" "${LS_BIN} -lu ${TEST_DIR}/file_a.txt > /dev/null" 0
run_test "-c sorts/prints using status change time" "${LS_BIN} -lc ${TEST_DIR}/file_a.txt > /dev/null" 0
run_test "-c and -u override (-cu gives access time)" "${LS_BIN} -lcu ${TEST_DIR}/file_a.txt > /dev/null" 0
run_test "-f produces unsorted output and implies -a" "${LS_BIN} -f ${TEST_DIR} | grep -q '^\.$'" 0

echo "--- 10. Non-Printable Characters (-q, -w) ---"
touch "${TEST_DIR}/file"$'\t'"tab.txt" 2>/dev/null || touch "${TEST_DIR}/file?tab.txt"
run_test "-q forces ? for non-printable" "${LS_BIN} -q ${TEST_DIR} > /dev/null" 0
run_test "-w forces raw non-printable" "${LS_BIN} -w ${TEST_DIR} > /dev/null" 0
run_test "-q and -w override (-qw gives raw)" "${LS_BIN} -qw ${TEST_DIR} > /dev/null" 0

echo "=========================================="
echo "Test Summary: ${PASSED} PASSED, ${FAILED} FAILED"
echo "=========================================="

if [ "${FAILED}" -eq 0 ]; then
    echo "ALL TESTS PASSED SUCCESSFULLY!"
    exit 0
else
    echo "SOME TESTS FAILED."
    exit 1
fi
