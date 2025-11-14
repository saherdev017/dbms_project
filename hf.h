#ifndef HF_H
#define HF_H

#include "pf.h" // We need this for PF_PAGE_SIZE

/*
 * The HF_PageHeader is the first thing on a slotted page.
 * It sits at offset 0.
 */
typedef struct {
    int numSlots;       /* Number of slots on this page */
    int dataStartPtr;   /* Offset from page start where data heap begins */
                        /* Data heap grows backwards (towards the front) */
} HF_PageHeader;

/*
 * A SlotEntry lives in the "slot array" that starts right
 * after the HF_PageHeader. The slot array grows forwards.
 */
typedef struct {
    int offset;         /* Offset from page start to the record's data */
    int length;         /* Length of the record */
                        /* If length == -1, the slot is free (deleted) */
} HF_SlotEntry;


/*
 * Define some error codes for the HF layer
 */
#define HFE_OK            0    /* OK */
#define HFE_PAGENOFREE    -20   /* Page has no free space */
#define HFE_INVALIDSLOT   -21   /* Invalid slot number */
#define HFE_EOF           -22   /* End of file */

/*
 * Function prototypes for the HF layer
 */

// Initializes a new slotted page
void HF_InitPage(char *pageBuf);

// Inserts a new record
int HF_InsertRec(char *pageBuf, char *record, int recLen);

// Deletes a record
int HF_DeleteRec(char *pageBuf, int slotNum);

// Gets a specific record
int HF_GetRec(char *pageBuf, int slotNum, char **record, int *recLen);

// Gets the next valid record
int HF_GetNextRec(char *pageBuf, int currentSlotNum, char **record, int *recLen);

#endif // HF_H