#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"    // Include the PF layer
#include "hf.h"    // Include our new HF layer

#define TEST_FILE "testfile"

int main() {
    int fd;
    int pagenum;
    char *pageBuf;
    int error;
    int slot1, slot2, slot3;

    printf("Starting HF layer test...\n\n");

    // 1. Initialize the PF layer
    PF_Init();

    // 2. Create and open a test file
    if ((error = PF_CreateFile(TEST_FILE)) != PFE_OK) {
        PF_PrintError("PF_CreateFile");
        exit(1);
    }
    if ((fd = PF_OpenFile(TEST_FILE)) < 0) {
        PF_PrintError("PF_OpenFile");
        exit(1);
    }

    // 3. Allocate a new page
    if ((error = PF_AllocPage(fd, &pagenum, &pageBuf)) != PFE_OK) {
        PF_PrintError("PF_AllocPage");
        exit(1);
    }

    // 4. Initialize it as an empty slotted page
    HF_InitPage(pageBuf);
    printf("Initialized new slotted page (Page %d)\n", pagenum);

    // 5. Define some records and insert them
    char *rec1 = "This is the first record.";
    char *rec2 = "This is a slightly longer, second record.";
    char *rec3 = "This is record three.";

    slot1 = HF_InsertRec(pageBuf, rec1, strlen(rec1) + 1); // +1 for null terminator
    slot2 = HF_InsertRec(pageBuf, rec2, strlen(rec2) + 1);
    slot3 = HF_InsertRec(pageBuf, rec3, strlen(rec3) + 1);

    if (slot1 < 0 || slot2 < 0 || slot3 < 0) {
        printf("Error inserting record (code: %d)\n", slot1);
        exit(1);
    }

    printf("Inserted 3 records at slots: %d, %d, %d\n\n", slot1, slot2, slot3);

    // 6. Delete the middle record (slot 2)
    printf("Deleting record at slot %d...\n", slot2);
    if ((error = HF_DeleteRec(pageBuf, slot2)) != HFE_OK) {
        printf("Error deleting record (code: %d)\n", error);
        exit(1);
    }
    printf("Delete successful.\n\n");

    // 7. Scan the page for all remaining records
    printf("Scanning page for remaining records:\n");
    int currentSlot = -1;
    char *recordData;
    int recordLen;

    while ((currentSlot = HF_GetNextRec(pageBuf, currentSlot, &recordData, &recordLen)) != HFE_EOF) {
        printf("  Found record at slot %d (length %d): '%s'\n", 
               currentSlot, recordLen, recordData);
    }
    printf("\nEnd of scan.\n\n");

    // 8. Test HF_GetRec directly
    printf("Testing HF_GetRec on slot %d (should be valid)...\n", slot3);
    if (HF_GetRec(pageBuf, slot3, &recordData, &recordLen) == HFE_OK) {
        printf("  Got record: '%s'\n", recordData);
    } else {
        printf("  Failed to get record!\n");
    }

    printf("Testing HF_GetRec on slot %d (should be deleted)...\n", slot2);
    if (HF_GetRec(pageBuf, slot2, &recordData, &recordLen) != HFE_OK) {
        printf("  Correctly failed to get deleted record.\n");
    } else {
        printf("  Error! Got a deleted record!\n");
    }
    printf("\n");


    // 9. Clean up the PF layer
    // Mark page as dirty (we changed it) and unfix it
    if ((error = PF_UnfixPage(fd, pagenum, TRUE)) != PFE_OK) {
        PF_PrintError("PF_UnfixPage");
        exit(1);
    }

    // Close and destroy the file
    if ((error = PF_CloseFile(fd)) != PFE_OK) {
        PF_PrintError("PF_CloseFile");
        exit(1);
    }
    if ((error = PF_DestroyFile(TEST_FILE)) != PFE_OK) {
        PF_PrintError("PF_DestroyFile");
        exit(1);
    }

    printf("HF layer test complete. Cleaned up files.\n");
    return 0;
}