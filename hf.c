#include "hf.h"
#include <stdio.h>
#include <string.h> // for memcpy

/*
 * Helper function to get a pointer to the header of a page.
 */
static HF_PageHeader* HF_GetPageHeader(char *pageBuf) {
    return (HF_PageHeader*)pageBuf;
}

/*
 * Helper function to get a pointer to the slot array on a page.
 */
static HF_SlotEntry* HF_GetSlotArray(char *pageBuf) {
    // The slot array starts right after the header
    return (HF_SlotEntry*)(pageBuf + sizeof(HF_PageHeader));
}

/*
 * Initializes a new, empty slotted page.
 * This is called by the PF layer right after allocating a new page.
 */
void HF_InitPage(char *pageBuf) {
    HF_PageHeader *header = HF_GetPageHeader(pageBuf);
    
    // This page has no records yet
    header->numSlots = 0; 
    
    // The data heap starts at the very end of the page
    // and grows "backward" (towards the front)
    header->dataStartPtr = PF_PAGE_SIZE;
}

/*
 * Inserts a new record onto the page.
 * Returns the new slot number if successful.
 * Returns an error code if it fails.
 */
int HF_Page_InsertRec(char *pageBuf, char *record, int recLen) {
    HF_PageHeader *header = HF_GetPageHeader(pageBuf);
    HF_SlotEntry *slotArray = HF_GetSlotArray(pageBuf);
    
    // 1. Calculate the end of the slot array
    int slotArrayEnd = sizeof(HF_PageHeader) + (header->numSlots * sizeof(HF_SlotEntry));
    
    // 2. Calculate the available free space
    //    (Space between the data heap and the slot array)
    int freeSpace = header->dataStartPtr - slotArrayEnd;
    
    // 3. Calculate space needed for this new record
    //    (The record's data + one new slot entry)
    int spaceNeeded = recLen + sizeof(HF_SlotEntry);

    // 4. Check if there is enough space
    if (freeSpace < spaceNeeded) {
        // Not enough free space on this page
        return HFE_PAGENOFREE;
    }
    
    // --- We have enough space, so let's insert ---

    // 5. Find the new record's destination
    //    (Move the data pointer "back" by recLen)
    header->dataStartPtr -= recLen;
    
    // 6. Copy the record data into the data heap
    memcpy(pageBuf + header->dataStartPtr, record, recLen);
    
    // 7. Get the new slot for this record
    //    (It's the next available slot in the array)
    HF_SlotEntry *newSlot = &slotArray[header->numSlots];
    
    // 8. Update the new slot's info
    newSlot->offset = header->dataStartPtr;
    newSlot->length = recLen;
    
    // 9. Update the header
    int newSlotNum = header->numSlots;
    header->numSlots++;
    
    // Return the slot number where we inserted the record
    return newSlotNum;
}

/*
 * Deletes a record from the page, given its slot number.
 *
 * This is a "lazy" delete: it just marks the slot as free
 * by setting its length to -1. It does not reclaim the space
 * or compact the page.
 */
int HF_Page_DeleteRec(char *pageBuf, int slotNum) {
    HF_PageHeader *header = HF_GetPageHeader(pageBuf);
    HF_SlotEntry *slotArray = HF_GetSlotArray(pageBuf);

    // 1. Check if the slot number is valid
    if (slotNum < 0 || slotNum >= header->numSlots) {
        return HFE_INVALIDSLOT;
    }

    HF_SlotEntry *slot = &slotArray[slotNum];

    // 2. Check if it's already deleted
    if (slot->length == -1) {
        return HFE_INVALIDSLOT;
    }

    // 3. "Delete" the record by invalidating its slot
    slot->length = -1;

    return HFE_OK;
}

/*
 * Retrieves a specific record from a page.
 *
 * Inputs:
 * pageBuf: Pointer to the page data
 * slotNum: The slot of the record to retrieve
 *
 * Outputs:
 * record: A pointer to the record data *within the page buffer*
 * recLen: The length of the record
 *
 * Returns:
 * HFE_OK if successful
 * HFE_INVALIDSLOT if the slot is invalid or deleted
 */
int HF_Page_GetRec(char *pageBuf, int slotNum, char **record, int *recLen) {
    HF_PageHeader *header = HF_GetPageHeader(pageBuf);
    HF_SlotEntry *slotArray = HF_GetSlotArray(pageBuf);

    // 1. Check if the slot number is valid
    if (slotNum < 0 || slotNum >= header->numSlots) {
        return HFE_INVALIDSLOT;
    }

    HF_SlotEntry *slot = &slotArray[slotNum];

    // 2. Check if the slot is deleted
    if (slot->length == -1) {
        return HFE_INVALIDSLOT;
    }

    // 3. Set the output pointers
    *record = pageBuf + slot->offset;
    *recLen = slot->length;

    return HFE_OK;
}


/*
 * Scans the page for the next valid record.
 *
 * Inputs:
 * pageBuf: Pointer to the page data
 * currentSlotNum: The slot number to *start scanning from*
 * (pass -1 to start from the beginning)
 *
 * Outputs:
 * record: A pointer to the next record's data
 * recLen: The length of the next record
 *
 * Returns:
 * The slot number of the record found.
 * HFE_EOF if no more valid records are found.
 */
int HF_Page_GetNextRec(char *pageBuf, int currentSlotNum, char **record, int *recLen) {
    HF_PageHeader *header = HF_GetPageHeader(pageBuf);
    HF_SlotEntry *slotArray = HF_GetSlotArray(pageBuf);

    // 1. Start scanning from the *next* slot
    for (int i = currentSlotNum + 1; i < header->numSlots; i++) {
        
        // 2. Check if this slot is valid (not deleted)
        if (slotArray[i].length != -1) {
            
            // 3. Found a valid record. Set output pointers.
            *record = pageBuf + slotArray[i].offset;
            *recLen = slotArray[i].length;
            
            // 4. Return the slot number we found it in
            return i;
        }
    }

    // 5. No more valid records were found on this page
    return HFE_EOF;
}

/*
 * ======================================================
 * File-level HF Layer Function Implementations
 * ======================================================
 */

/*
 * Creates a new, empty heap file.
 * This is just a wrapper for the PF layer.
 */
int HF_CreateFile(char *fileName) {
    // Call the PF layer to create the file
    if (PF_CreateFile(fileName) != PFE_OK) {
        return PFerrno; // Return PF layer's error code
    }
    return HFE_OK;
}

/*
 * Opens an existing heap file.
 * This is just a wrapper for the PF layer.
 * Returns a file descriptor (fd) from the PF layer.
 */
int HF_OpenFile(char *fileName) {
    // Call the PF layer to open the file
    int fd = PF_OpenFile(fileName);
    if (fd < 0) {
        return PFerrno; // Return PF layer's error code
    }
    return fd; // Return the file descriptor
}

/*
 * Closes a heap file.
 * This is just a wrapper for the PF layer.
 */
int HF_CloseFile(int fd) {
    // Call the PF layer to close the file
    if (PF_CloseFile(fd) != PFE_OK) {
        return PFerrno; // Return PF layer's error code
    }
    return HFE_OK;
}

/*
 * Inserts a record into the file.
 *
 * This function scans the file page by page to find one
 * with enough free space. If no page has space, it
 * allocates a new page and inserts the record there.
 */
int HF_InsertRec(int fd, char *record, int recLen, RID *rid) {
    int pagenum = -1; // Start scan from the beginning
    char *pageBuf;
    int error;
    int slotNum;

    // 1. Scan the file for a page with free space
    while ((error = PF_GetNextPage(fd, &pagenum, &pageBuf)) == PFE_OK) {
        
        // Try to insert the record on this page
        // (We call our old page-level insert function)
        slotNum = HF_Page_InsertRec(pageBuf, record, recLen);
        
        if (slotNum == HFE_PAGENOFREE) {
            // This page is full, unfix it and try the next one
            if ((error = PF_UnfixPage(fd, pagenum, FALSE)) != PFE_OK) {
                return error; // Propagate PF error
            }
            continue; // Go to the next page
        }
        
        // --- Success! We found space and inserted the record ---
        
        // Set the output RID
        rid->pageNum = pagenum;
        rid->slotNum = slotNum;
        
        // Mark the page as dirty (it was modified) and unfix it
        if ((error = PF_UnfixPage(fd, pagenum, TRUE)) != PFE_OK) {
            return error;
        }
        
        // Return success
        return HFE_OK;
    }
    
    // 2. We reached here, so PF_GetNextPage failed.
    // Check if it was because we reached the End Of File (EOF).
    if (error != PFE_EOF) {
        return error; // It was a real error
    }
    
    // 3. --- No page had free space, so we must allocate a new one ---
    
    // Allocate a new page
    if ((error = PF_AllocPage(fd, &pagenum, &pageBuf)) != PFE_OK) {
        return error; // Propagate PF error
    }
    
    // Initialize the new page
    HF_InitPage(pageBuf);
    
    // Insert the record (this *must* succeed on a new page)
    slotNum = HF_Page_InsertRec(pageBuf, record, recLen);
    
    // Set the output RID
    rid->pageNum = pagenum;
    rid->slotNum = slotNum;
    
    // Mark the new page as dirty and unfix it
    if ((error = PF_UnfixPage(fd, pagenum, TRUE)) != PFE_OK) {
        return error;
    }

    return HFE_OK;
}

/*
 * Deletes a record from the file, given its RID.
 */
int HF_DeleteRec(int fd, RID rid) {
    char *pageBuf;
    int error;
    
    // 1. Get the specific page the record is on
    if ((error = PF_GetThisPage(fd, rid.pageNum, &pageBuf)) != PFE_OK) {
        return error;
    }
    
    // 2. Call our page-level delete function
    error = HF_Page_DeleteRec(pageBuf, rid.slotNum);
    
    // 3. Mark the page as dirty and unfix it
    if (PF_UnfixPage(fd, rid.pageNum, TRUE) != PFE_OK) {
        return PFE_UNIX; // Return a generic error if unfix fails
    }
    
    return error; // Return result of HF_DeleteRec
}

/*
 * Retrieves a record from the file, given its RID.
 *
 * Outputs:
 * record: Pointer to the record data *within the buffer page*
 * recLen: Length of the record
 *
 * WARNING: The 'record' pointer is only valid until the page
 * is unfixed. The caller must copy the data if needed.
 */
int HF_GetRec(int fd, RID rid, char **record, int *recLen) {
    char *pageBuf;
    int error;

    // 1. Get the specific page the record is on
    if ((error = PF_GetThisPage(fd, rid.pageNum, &pageBuf)) != PFE_OK) {
        return error;
    }

    // 2. Call our page-level get function
    error = HF_Page_GetRec(pageBuf, rid.slotNum, record, recLen);

    // 3. Unfix the page (it wasn't modified)
    if (PF_UnfixPage(fd, rid.pageNum, FALSE) != PFE_OK) {
        return PFE_UNIX;
    }

    return error; // Return result of HF_Page_GetRec
}

/*
 * ======================================================
 * File Scan Function Implementations
 * ======================================================
 */

/*
 * Opens a new file scan.
 *
 * Initializes the scan struct to start from the beginning
 * of the file.
 */
int HF_OpenFileScan(int fd, HF_Scan *scan) {
    // Store the file descriptor
    scan->fd = fd;
    
    // Start the scan *before* the first page
    scan->currentPageNum = -1; 
    
    // No record has been found yet
    scan->currentSlotNum = -1;
    
    // No page is pinned in the buffer yet
    scan->currentPageBuf = NULL;
    
    return HFE_OK;
}

/*
 * Closes a file scan.
 *
 * This function must be called to unfix the last page
 * that the scanner was looking at.
 */
int HF_CloseFileScan(HF_Scan *scan) {
    int error;
    
    // Check if a page is still pinned in the buffer
    if (scan->currentPageBuf != NULL) {
        // Unfix it (it wasn't modified)
        error = PF_UnfixPage(scan->fd, scan->currentPageNum, FALSE);
        if (error != PFE_OK) {
            return error;
        }
    }
    
    // Reset the scan struct
    scan->fd = -1;
    scan->currentPageBuf = NULL;
    
    return HFE_OK;
}

/*
 * Retrieves the next valid record in the file scan.
 */
/*
 * Retrieves the next valid record in the file scan.
 */
int HF_GetNextRec(int fd, HF_Scan *scan, RID *rid, char **record, int *recLen) {
    int error;

    // This is the main scanner loop
    while (TRUE) {
        
        // --- 1. Try to find the next record on the current page ---
        
        // Check if we have a page loaded
        if (scan->currentPageBuf != NULL) {
            
            // Call our page-level scanner. It returns the slot number.
            int slot = HF_Page_GetNextRec(scan->currentPageBuf, 
                                        scan->currentSlotNum, 
                                        record, 
                                        recLen);
            
            if (slot >= 0) { // HFE_OK is 0, but this is safer
                // --- Success! Found a record on this page ---
                
                // Update the scanner's state
                scan->currentSlotNum = slot;
                
                // Set the output RID
                rid->pageNum = scan->currentPageNum;
                rid->slotNum = scan->currentSlotNum;
                
                return HFE_OK;
            }
            
            // If we're here, the result was HFE_EOF (no more records on this page)
            // Unfix the current page
            if ((error = PF_UnfixPage(scan->fd, scan->currentPageNum, FALSE)) != PFE_OK) {
                return error; // Propagate error
            }
            scan->currentPageBuf = NULL;
        }

        // --- 2. Get the next page in the file ---
        
        // PF_GetNextPage gets the page *after* scan->currentPageNum
        error = PF_GetNextPage(scan->fd, &scan->currentPageNum, &scan->currentPageBuf);
        
        if (error == PFE_EOF) {
            // --- End of File ---
            // No more pages left to scan
            return HFE_EOF;
        }
        
        if (error != PFE_OK) {
            // A real PF error occurred
            return error;
        }

        // --- 3. Got a new page, reset page scanner ---
        
        // Reset the slot number to start scanning this new page
        // from the beginning in the next loop iteration.
        scan->currentSlotNum = -1;
    }
}