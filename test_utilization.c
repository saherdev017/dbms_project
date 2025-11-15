#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "hf.h"

#define TEST_FILE "utilization.data"
#define RECORD_SIZE 100    // The size of each record we'll insert
#define NUM_RECORDS 1000   // The number of records to insert

int main() {
    int fd;
    int error;
    char *record = malloc(RECORD_SIZE);
    RID last_rid;
    int i;
    
    // Create a dummy record
    memset(record, 'a', RECORD_SIZE);

    printf("Starting Space Utilization Test...\n");

    // 1. Init PF layer
    PF_Init();

    // 2. Create and Open file
    if ((error = HF_CreateFile(TEST_FILE)) != HFE_OK) {
        PF_PrintError("HF_CreateFile");
        exit(1);
    }
    if ((fd = HF_OpenFile(TEST_FILE)) < 0) {
        PF_PrintError("HF_OpenFile");
        exit(1);
    }

    // 3. Insert records
    printf("Inserting %d records of size %d bytes each...\n", NUM_RECORDS, RECORD_SIZE);
    for (i = 0; i < NUM_RECORDS; i++) {
        // We only care about the RID of the *last* record
        if ((error = HF_InsertRec(fd, record, RECORD_SIZE, &last_rid)) != HFE_OK) {
            printf("Error inserting record %d (code: %d)\n", i, error);
            exit(1);
        }
    }
    printf("Insertions complete.\n\n");

    // 4. Close the file
    if ((error = HF_CloseFile(fd)) != HFE_OK) {
        PF_PrintError("HF_CloseFile");
        exit(1);
    }

    // 5. Calculate and print results
    // Page numbers are 0-indexed, so we add 1
    int totalPagesUsed = last_rid.pageNum + 1;
    long long totalDataBytes = (long long)NUM_RECORDS * RECORD_SIZE;
    long long totalFileBytes = (long long)totalPagesUsed * PF_PAGE_SIZE;
    
    // Calculate utilization (remember to cast to double for floating-point division)
    double utilization = ((double)totalDataBytes / (double)totalFileBytes) * 100.0;

    printf("--- Space Utilization Results ---\n");
    printf("Record Size:          %d bytes\n", RECORD_SIZE);
    printf("Total Records:        %d\n", NUM_RECORDS);
    printf("----------------------------------\n");
    printf("Total Useful Data:    %lld bytes\n", totalDataBytes);
    printf("Total Pages Used:     %d\n", totalPagesUsed);
    printf("Total File Space:     %lld bytes (%d * %d)\n", totalFileBytes, totalPagesUsed, PF_PAGE_SIZE);
    printf("----------------------------------\n");
    printf("Space Utilization:    %.2f %%\n", utilization);

    // 6. Clean up
    if ((error = PF_DestroyFile(TEST_FILE)) != PFE_OK) {
        PF_PrintError("PF_DestroyFile");
        exit(1);
    }

    free(record);
    printf("\nTest complete. Cleaned up file.\n");
    return 0;
}