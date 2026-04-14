#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "--- TEST FAILED: %s ---\n", message); \
            abort(); \
        } else { \
            printf("[PASS] %s\n", message); \
        } \
    } while (0)

void pipe_test() {

    int fds[2];
    ssize_t bytes_read;
    char read_buf[100];

    // --- Test 1: Basic Successful Write and Read Cycle ---
    printf("\n--- Running Test 1: Basic Write/Read ---\n");
    if (pipe(fds) == -1) {
        perror("pipe failed for Test 1");
        assert(0); // Force failure if pipe setup fails
    }
    
    // Write data
    const char *msg1 = "Hello World";
    ssize_t written = write(fds[1], msg1, strlen(msg1) + 1); // +1 for null terminator
    TEST_ASSERT(written == strlen(msg1) + 1, "Test 1 Write: Must write exact number of bytes.");

    // Read data
    bytes_read = read(fds[0], read_buf, sizeof(read_buf) - 1);
    read_buf[bytes_read] = '\0'; // Null terminate the read buffer
    
    TEST_ASSERT(bytes_read > 0, "Test 1 Read: Must read at least one byte.");
    TEST_ASSERT(strcmp(read_buf, msg1) == 0, "Test 1 Read: Read data must match written data.");

    // Cleanup for Test 1
    close(fds[0]);
    close(fds[1]);


    // --- Test 2: Writing and Reading Empty Data ---
    printf("\n--- Running Test 2: Empty Write/Read Cycle ---\n");
    if (pipe(fds) == -1) {
        perror("pipe failed for Test 2");
        assert(0);
    }
    
    // Write nothing (optional, but good practice)
    write(fds[1], "A", 1);
    close(fds[1]); // Close writing end immediately

    // Read attempt: Should block or return 0 immediately if the writer closes
    bytes_read = read(fds[0], read_buf, sizeof(read_buf) - 1);
    TEST_ASSERT(bytes_read == 0, "Test 2 Read: Reading from a closed write end must result in 0 bytes.");
    
    // Cleanup for Test 2
    close(fds[0]);
    // Note: We don't close fds[1] here as it's already conceptually closed by the writer side test

    
    // --- Test 3: Multiple Writes and Partial Reads ---
    printf("\n--- Running Test 3: Multiple Writes & Partial Reads ---\n");
    if (pipe(fds) == -1) {
        perror("pipe failed for Test 3");
        assert(0);
    }
    
    // Write distinct blocks of data
    const char *msg2 = "Alpha";
    const char *msg3 = "Beta";
    
    write(fds[1], msg2, strlen(msg2) + 1);
    write(fds[1], msg3, strlen(msg3) + 1);
    
    // Close the writing end (critical step for reliable EOF detection)
    close(fds[1]);
    
    // First read: Should get the first message
    bytes_read = read(fds[0], read_buf, sizeof(read_buf) - 1);
    read_buf[bytes_read] = '\0';
    TEST_ASSERT(bytes_read > 0, "Test 3 Read 1: Must read the first message.");
    TEST_ASSERT(strcmp(read_buf, msg2) == 0, "Test 3 Read 1: Read data must match 'Alpha'.");

    // Second read: Should get the second message
    bytes_read = read(fds[0], read_buf, sizeof(read_buf) - 1);
    read_buf[bytes_read] = '\0';
    TEST_ASSERT(bytes_read > 0, "Test 3 Read 2: Must read the second message.");
    TEST_ASSERT(strcmp(read_buf, msg3) == 0, "Test 3 Read 2: Read data must match 'Beta'.");

    // Third read: Should hit EOF
    bytes_read = read(fds[0], read_buf, sizeof(read_buf) - 1);
    TEST_ASSERT(bytes_read == 0, "Test 3 Read 3: After all data is read and writer closed, read must return 0.");

    // Cleanup for Test 3
    close(fds[0]);

    
    // --- Test 4: Robustness Check - Writing to Closed Pipe (Simulated Error) ---
    printf("\n--- Running Test 4: Write After Close (Expected Error) ---\n");
    if (pipe(fds) == -1) {
        perror("pipe failed for Test 4 setup");
        assert(0);
    }
    
    // Immediately close the write end
    close(fds[1]);
    
    // Attempt to write. This should fail with EPIPE.
    ssize_t write_result = write(fds[1], "Fail", 4);
    
    // We expect write to return -1 and errno to be EPIPE
    TEST_ASSERT(write_result == -1, "Test 4 Write: Writing to a closed pipe must return -1.");
    
    // Cleanup for Test 4
    close(fds[0]);
    
    printf("\n========================================\n");
    printf("All Pipe Tests Completed Successfully!\n");
    printf("========================================\n");
}
