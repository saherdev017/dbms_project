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
int HF_InsertRec(char *pageBuf, char *record, int recLen) {
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
int HF_DeleteRec(char *pageBuf, int slotNum) {
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
int HF_GetRec(char *pageBuf, int slotNum, char **record, int *recLen) {
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
int HF_GetNextRec(char *pageBuf, int currentSlotNum, char **record, int *recLen) {
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