#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "hf.h"

#define TEST_FILE_HF "testhf3.data"
#define NUM_RECORDS 150 // Let's use a number that's not a round 100

int main() {
    int fd;
    int error;
    char record[100];
    RID rid;
    RID *rids = malloc(sizeof(RID) * NUM_RECORDS);
    char *recordData;
    int recordLen;
    int i;
    int recordsFound = 0;

    printf("Starting HF scanner test (testhf3)...\n\n");

    // 1. Init PF layer
    PF_Init();

    // 2. Create and Open file
    if ((error = HF_CreateFile(TEST_FILE_HF)) != HFE_OK) {
        PF_PrintError("HF_CreateFile");
        exit(1);
    }
    if ((fd = HF_OpenFile(TEST_FILE_HF)) < 0) {
        PF_PrintError("HF_OpenFile");
        exit(1);
    }
    printf("Created and opened file: %s (fd: %d)\n", TEST_FILE_HF, fd);

    // 3. Insert records
    printf("Inserting %d records...\n", NUM_RECORDS);
    for (i = 0; i < NUM_RECORDS; i++) {
        sprintf(record, "This is record number %d.", i);
        if ((error = HF_InsertRec(fd, record, strlen(record) + 1, &rids[i])) != HFE_OK) {
            printf("Error inserting record %d (code: %d)\n", i, error);
            exit(1);
        }
    }
    printf("Successfully inserted %d records.\n\n", NUM_RECORDS);

    // 4. Delete a few records
    // Let's delete record 10, 50, and 100
    RID rid_10 = rids[10];
    RID rid_50 = rids[50];
    RID rid_100 = rids[100];
    int deleteCount = 3;

    printf("Deleting record 10 (RID: Page %d, Slot %d)\n", rid_10.pageNum, rid_10.slotNum);
    HF_DeleteRec(fd, rid_10);
    
    printf("Deleting record 50 (RID: Page %d, Slot %d)\n", rid_50.pageNum, rid_50.slotNum);
    HF_DeleteRec(fd, rid_50);
    
    printf("Deleting record 100 (RID: Page %d, Slot %d)\n", rid_100.pageNum, rid_100.slotNum);
    HF_DeleteRec(fd, rid_100);
    printf("Deleted %d records.\n\n", deleteCount);


    // 5. Scan the file
    printf("Starting file scan...\n");
    HF_Scan scan;
    if ((error = HF_OpenFileScan(fd, &scan)) != HFE_OK) {
        PF_PrintError("HF_OpenFileScan");
        exit(1);
    }

    while ((error = HF_GetNextRec(fd, &scan, &rid, &recordData, &recordLen)) == HFE_OK) {
        printf("  Found RID (Page %d, Slot %d): '%s'\n", 
               rid.pageNum, rid.slotNum, recordData);
        recordsFound++;

        // Check for deleted records (this should not happen)
        if (strcmp(recordData, "This is record number 10.") == 0 ||
            strcmp(recordData, "This is record number 50.") == 0 ||
            strcmp(recordData, "This is record number 100.") == 0) {
            printf("  *** ERROR: Found a deleted record! ***\n");
        }
    }

    if (error != HFE_EOF) {
        printf("Error during scan (code: %d)\n", error);
    }
    printf("...End of file scan.\n\n");

    // 6. Close the scan
    HF_CloseFileScan(&scan);

    // 7. Check the final count
    printf("--- Scan Summary ---\n");
    printf("Total records inserted: %d\n", NUM_RECORDS);
    printf("Total records deleted:  %d\n", deleteCount);
    printf("Total records found:    %d\n", recordsFound);
    
    if (recordsFound == (NUM_RECORDS - deleteCount)) {
        printf("SUCCESS! The scanner found the correct number of records.\n\n");
    } else {
        printf("FAILURE! The scanner found %d, but should have found %d.\n\n",
               recordsFound, (NUM_RECORDS - deleteCount));
    }

    // 8. Clean up
    if ((error = HF_CloseFile(fd)) != HFE_OK) {
        PF_PrintError("HF_CloseFile");
        exit(1);
    }
    if ((error = PF_DestroyFile(TEST_FILE_HF)) != PFE_OK) {
        PF_PrintError("PF_DestroyFile");
        exit(1);
    }

    printf("HF scanner test complete. Cleaned up files.\n");
    free(rids);
    return 0;
}
