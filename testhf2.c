/*This program will be the "master test" for your entire HF layer. 
It will verify that you can create a file, 
fill it up with enough records to force it to allocate new pages, 
and then successfully retrieve and delete records using the RID.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "hf.h"

#define TEST_FILE_HF "testhf2.data"
#define NUM_RECORDS 300 // Number of records to insert

int main() {
    int fd;
    int error;
    char record[100]; // Buffer for a sample record
    RID rid;
    RID *rids = malloc(sizeof(RID) * NUM_RECORDS); // Array to store all RIDs
    char *recordData;
    int recordLen;
    int i;

    printf("Starting HF file-level test...\n\n");

    // 1. Initialize the PF layer (needed by HF layer)
    PF_Init();

    // 2. Create a new heap file
    if ((error = HF_CreateFile(TEST_FILE_HF)) != HFE_OK) {
        PF_PrintError("HF_CreateFile");
        exit(1);
    }

    // 3. Open the heap file
    if ((fd = HF_OpenFile(TEST_FILE_HF)) < 0) {
        PF_PrintError("HF_OpenFile");
        exit(1);
    }
    printf("Created and opened file: %s (fd: %d)\n", TEST_FILE_HF, fd);

    // 4. Insert a bunch of records
    printf("Inserting %d records...\n", NUM_RECORDS);
    for (i = 0; i < NUM_RECORDS; i++) {
        // Create a unique record
        sprintf(record, "This is record number %d.", i);

        // Insert it and store its RID
        if ((error = HF_InsertRec(fd, record, strlen(record) + 1, &rids[i])) != HFE_OK) {
            printf("Error inserting record %d (code: %d)\n", i, error);
            exit(1);
        }
    }
    printf("Successfully inserted %d records.\n\n", NUM_RECORDS);

    // 5. Test HF_GetRec: Pick a record from the middle and get it
    int recordToTest = NUM_RECORDS / 2;
    RID testRID = rids[recordToTest];
    
    printf("Testing HF_GetRec on record %d (RID: Page %d, Slot %d)...\n", 
           recordToTest, testRID.pageNum, testRID.slotNum);

    if (HF_GetRec(fd, testRID, &recordData, &recordLen) == HFE_OK) {
        printf("  Got record: '%s'\n", recordData);
        // Let's also check if the content is correct
        sprintf(record, "This is record number %d.", recordToTest);
        if (strcmp(record, recordData) == 0) {
            printf("  Record content is correct!\n\n");
        } else {
            printf("  ERROR: Record content is INCORRECT!\n\n");
        }
    } else {
        printf("  Failed to get record %d!\n\n", recordToTest);
    }

    // 6. Test HF_DeleteRec: Delete the record we just got
    printf("Testing HF_DeleteRec on record %d...\n", recordToTest);
    if ((error = HF_DeleteRec(fd, testRID)) != HFE_OK) {
        printf("  Failed to delete record (code: %d)!\n\n", error);
        exit(1);
    }
    printf("  Delete successful.\n\n");

    // 7. Test HF_GetRec again (it should fail)
    printf("Testing HF_GetRec on deleted record %d...\n", recordToTest);
    if (HF_GetRec(fd, testRID, &recordData, &recordLen) == HFE_OK) {
        printf("  ERROR! We got a deleted record!\n\n");
    } else {
        printf("  Correctly failed to get deleted record.\n\n");
    }

    // 8. Close the file
    if ((error = HF_CloseFile(fd)) != HFE_OK) {
        PF_PrintError("HF_CloseFile");
        exit(1);
    }

    // 9. Clean up (destroy the file)
    if ((error = PF_DestroyFile(TEST_FILE_HF)) != PFE_OK) {
        PF_PrintError("PF_DestroyFile");
        exit(1);
    }

    printf("HF file-level test complete. Cleaned up files.\n");
    free(rids);
    return 0;
}