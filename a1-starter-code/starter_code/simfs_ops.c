/* This file contains functions that are not part of the visible interface.
 * So they are essentially helper functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simfs.h"
#include "simfstypes.h"

/* Internal helper functions first.
 */

FILE *
openfs(char *filename, char *mode)
{
    FILE *fp;
    if((fp = fopen(filename, mode)) == NULL) {
        perror("openfs");
        exit(1);
    }
    return fp;
}

void
closefs(FILE *fp)
{
    if(fclose(fp) != 0) {
        perror("closefs");
        exit(1);
    }
}

/* File system operations: creating, deleting, reading from, and writing to files.
 */

// Signatures omitted; design as you wish.

int createfile(char *filename) {
    FILE *fp;
    struct fentry *fep;

    // Check if the file already exists
    if ((fep = findfentry(filename))!= NULL) {
        fprintf(stderr, "Error: File already exists\n");
        return -1;
    }

    // Allocate a new file entry
    if ((fep = allocfentry()) == NULL) {
        fprintf(stderr, "Error: Insufficient resources\n");
        return -1;
    }

    // Open the file
    if ((fp = openfs(filename, "w")) == NULL) {
        freefentry(fep);
        return -1;
    }

    // Initialize the file entry
    strcpy(fep->name, filename);
    fep->size = 0;
    fep->first_block = NULL;

    return 0;
}

int deletefile(char *filename) {
    FILE *fp;
    struct fentry *fep;
    struct block *bp, *next_bp;

    // Find the file entry
    if ((fep = findfentry(filename)) == NULL) {
        fprintf(stderr, "Error: File not found\n");
        return -1;
    }

    // Open the file
    if ((fp = openfs(filename, "r")) == NULL) {
        return -1;
    }

    // Free all data blocks
    for (bp = fep->first_block; bp!= NULL; bp = next_bp) {
        next_bp = bp->next;
        freeblock(bp);
    }

    // Remove the file entry
    removefentry(fep);

    // Close the file
    closefs(fp);

    // Overwrite the file with zeros
    if ((fp = openfs(filename, "w")) == NULL) {
        return -1;
    }
    for (int i = 0; i < fep->size; i++) {
        fputc(0, fp);
    }
    closefs(fp);

    return 0;
}

int readfile(char *filename, int start, int length) {
    FILE *fp;
    struct fentry *fep;
    struct block *bp;
    int pos, count;

    // Find the file entry
    if ((fep = findfentry(filename)) == NULL) {
        fprintf(stderr, "Error: File not found\n");
        return -1;
    }

    // Check the start position
    if (start < 0 || start >= fep->size) {
        fprintf(stderr, "Error: Invalid start position\n");
        return -1;
    }

    // Open the file
    if ((fp = openfs(filename, "r")) == NULL) {
        return -1;
    }

    // Seek to the start position
    if (fseek(fp, start, SEEK_SET)!= 0) {
        closefs(fp);
        return -1;
    }

    // Read the requested data
    for (pos = start, count = 0; count < length && pos < fep->size; pos++, count++) {
        if (fread(&bp, sizeof(bp), 1, fp)!= 1) {
            break;
        }
        printf("%c", bp->data[pos % BLOCKSIZE]);
    }

    // Close the file
    closefs(fp);

    return count;
}

int writefile(char *filename, int start, int length) {
    FILE *fp;
    struct fentry *fep;
    struct block *bp, *prev_bp, *new_bp;
    int pos, count;

    // Find the file entry
    if ((fep = findfentry(filename)) == NULL) {
        fprintf(stderr, "Error: File not found\n");
        return -1;
    }

    // Check the start position
    if (start < 0) {
        fprintf(stderr, "Error: Invalid start position\n");
        return -1;
    }

    // Open the file
    if ((fp = openfs(filename, "r+")) == NULL) {
        return -1;
    }

    // Seek to the start position
    if (fseek(fp, start, SEEK_SET)!= 0) {
        closefs(fp);
        return -1;
    }

    // Write the requested data
    for (pos = start, count = 0; count < length; pos++, count++) {
        // Allocate a new block if necessary
        if (pos % BLOCKSIZE == 0) {
            if ((new_bp = allocblock()) == NULL) {
                closefs(fp);
                return -1;
            }
            if (fep->first_block == NULL) {
                fep->first_block = new_bp;
            } else {
                prev_bp->next = new_bp;
            }
            prev_bp = new_bp;
        }

        // Read the block
        if (fread(&bp, sizeof(bp), 1, fp)!= 1) {
            break;
        }

        // Write the data
        bp->data[pos % BLOCKSIZE] = fgetc(stdin);

        // Write the block
        if (fseek(fp, -sizeof(bp), SEEK_CUR)!= 0) {
            break;
        }
        if (fwrite(&bp, sizeof(bp), 1, fp)!= 1) {
            break;
        }
    }

    // Update the file size
    fep->size = pos;

    // Close the file
    closefs(fp);

    return count;
}