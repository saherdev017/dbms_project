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


/* A Record ID (RID) uniquely identifies a record in the file.
 * It consists of the page number and the slot number.
 */
typedef struct {
    int pageNum;
    int slotNum;
} RID;
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
int HF_Page_InsertRec(char *pageBuf, char *record, int recLen);

// Deletes a record
int HF_Page_DeleteRec(char *pageBuf, int slotNum);

// Gets a specific record
int HF_Page_GetRec(char *pageBuf, int slotNum, char **record, int *recLen);

// Gets the next valid record
int HF_Page_GetNextRec(char *pageBuf, int currentSlotNum, char **record, int *recLen);

/*
 * Creates a new, empty heap file.
 */
int HF_CreateFile(char *fileName);

/*
 * Opens an existing heap file.
 * Returns a file descriptor (fd) from the PF layer.
 */
int HF_OpenFile(char *fileName);

/*
 * Closes a heap file.
 */
int HF_CloseFile(int fd);

/*
 * Inserts a record into the file.
 *
 * Inputs:
 * fd: The file descriptor
 * record: Pointer to the record data
 * recLen: Length of the record
 *
 * Outputs:
 * rid: The RID of the newly inserted record
 *
 * Returns:
 * HFE_OK on success, or an error code
 */
int HF_InsertRec(int fd, char *record, int recLen, RID *rid);

/*
 * Deletes a record from the file, given its RID.
 */
int HF_DeleteRec(int fd, RID rid);

/*
 * Retrieves a record from the file, given its RID.
 *
 * Outputs:
 * record: Pointer to the record data *within the buffer page*
 * recLen: Length of the record
 */
int HF_GetRec(int fd, RID rid, char **record, int *recLen);

/*
 * ======================================================
 * File Scan Function Prototypes
 * ======================================================
 */

// This struct will keep track of the scanner's state
typedef struct {
    int   fd;             // The file descriptor
    int   currentPageNum; // Page number of the current page
    int   currentSlotNum; // Slot number of the last record found
    char *currentPageBuf; // Pinned buffer for the current page
} HF_Scan;


/*
 * Opens a new file scan.
 *
 * Inputs:
 * fd: The file descriptor
 *
 * Outputs:
 * scan: A pointer to an HF_Scan struct that will be
 * initialized by this function.
 *
 * Returns:
 * HFE_OK on success
 */
int HF_OpenFileScan(int fd, HF_Scan *scan);

/*
 * Retrieves the next valid record in the file scan.
 *
 * Inputs:
 * scan: The HF_Scan struct initialized by OpenFileScan
 *
 * Outputs:
 * rid: The RID of the next record
 * record: Pointer to the record's data
 * recLen: Length of the record
 *
 * Returns:
 * HFE_OK on success
 * HFE_EOF when no more records are found
 */
int HF_GetNextRec(int fd, HF_Scan *scan, RID *rid, char **record, int *recLen);

/*
 * Closes a file scan.
 */
int HF_CloseFileScan(HF_Scan *scan);

#endif // HF_H