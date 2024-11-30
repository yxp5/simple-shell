#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset
#include "shell.h" // MAX_USER_INPUT
#include "shellmemory.h"
#include "pcb.h"

#ifndef FRAMESIZE
#define FRAMESIZE 18
#endif

#ifndef VARMEMSIZE
#define VARMEMSIZE 10
#endif

static int frame_store_size = 0;
static int frame_store[FRAMESIZE];

// Pretty self-explanatory
int get_frame_store_size() {
    return frame_store_size;
}

int* get_frame_store() {
    return frame_store;
}

// Check if [instr] is in the frame store [array]
int in_frame_store(int instr) {
    for (int i = 0; i < frame_store_size; i++) {
        if (instr == frame_store[i]) return i + 1;
    }
    return 0;
}

// Helper function to check if [num] is in [array] of size [size]
int in(int num, int* array, int size) {
    for (int i = 0; i < size; i++) {
        if (array[i] == num) return 1;
    }
    return 0;
}

// Find the least recent frame
int LR_frame() {
    int scanned[FRAMESIZE];
    int pointer = 0;
    int LR = -1;
    int frame;

    for (int i = 0; i < frame_store_size; i++) {
        frame = frame_store[i] / 3;
        if (pointer == 0) {
            // Case: base case, always append the first frame (only run once)
            LR = frame;
            scanned[pointer] = frame;
            pointer++;
        } else if (in(frame, scanned, pointer)) {
            // Case: already scanned, then we don't care
            continue;
        } else {
            // Case: not scanned => current least recent frame, then update LR
            LR = frame;
            scanned[pointer] = frame;
            pointer++;
        }
    }
    return LR;
}

void LRU_update(int instr) {
    int present = in_frame_store(instr);
    if (present) {
        int current  = instr;
        int next = frame_store[0];
        for (int i = 0; i < present; i++) {
            frame_store[i] = current;
            current = next;
            next = frame_store[i+1];
        }
    } else {
        present = frame_store_size;
        int current  = instr;
        int next = frame_store[0];
        for (int i = 0; i < present; i++) {
            frame_store[i] = current;
            current = next;
            next = frame_store[i+1];
        }
    }

}

void LRU_evict(int LR, int instr) {
    for (int i = 0; i < 3; i++) {
        LRU_update(LR+i);
        frame_store[0] = instr + i;
    }
}

void print_frame_store() {
    for (int i = 0; i < frame_store_size; i++) {
        printf("index: %d\tvalue: %d\n", i, frame_store[i]);
    }
}

int pcb_has_next_instruction(struct PCB *pcb) {
    // have next if pc < line_count.
    // Sanity check: count = 0  ==> never have next. Good!
    return pcb->pc < pcb->line_count;
}

size_t pcb_next_instruction(struct PCB *pcb) {
    size_t i = pcb->line_base + pcb->pc;
    pcb->pc++;
    return i;
}

struct PCB *create_process(const char *filename) {
    // We have 2 main tasks:
    // load all the code in the script file into shellmemory, and
    // allocate+fill a PCB.

    // We don't want to allocate a PCB until we know we actually need one,
    // so let's first make sure we can open the file.
    FILE *script = fopen(filename, "rt");
    if (!script) {
        perror("failed to open file for create_process");
        return NULL;
    }
    struct PCB *pcb = create_process_from_FILE(script);
    // Update the pcb name according to the filename we received.
    pcb->name = strdup(filename);
    return pcb;
}

struct PCB *create_process_from_FILE(FILE *script) {
    // We can open the file, so we'll be making a process.
    struct PCB *pcb = malloc(sizeof(struct PCB));

    // The PID is the only weird part. They need to be distinct,
    // so let's use a static counter.
    static pid fresh_pid = 1;
    pcb->pid = fresh_pid++;

    // name should be the empty string, according to doc comment.
    pcb->name = "";
    // next should be NULL, according to doc comment.
    pcb->next = NULL;

    // pc is always initially 0.
    pcb->pc = 0;

    // sc is always initially 1.
    pcb->sc = 1;
    // initialize pt, allocate -1 to signal the end of table
    pcb->pt[0] = -1;
    // initialize pointers at 0
    pcb->residue = 0;
    pcb->pointer = 0;

    // create initial values for base and count, in case we fail to read
    // any lines from the file. That way we'll end up with an empty process
    // that terminates as soon as it is scheduled -- reasonable behavior for
    // an empty script.
    // If we don't do this, and the loop below never runs, base would be
    // unitialized, which would be catastrophic.
    pcb->line_count = 0;
    pcb->line_base = 0;
    pcb->fault = 0;

    // We're told to assume lines of files are limited to 100 characters.
    // That's all well and good, but for implementing # we need to read
    // actual user input, and _that_ is limited to 1000 characters.
    // It's unclear if we should assume it's also limited to 100 for this
    // purpose. If you did assume that, that's OK! We didn't.
    char linebuf[MAX_USER_INPUT];
    int pointer = 0;
    while (!feof(script)) {
        memset(linebuf, 0, sizeof(linebuf));
        fgets(linebuf, MAX_USER_INPUT, script);

        // Skip empty lines
        if (strspn(linebuf, " \t\v\f\r\n") == strlen(linebuf)) continue;
        size_t index = allocate_line(linebuf);
        // If we've run out of memory, clean up the partially-allocated
        // pcb and return NULL.
        if (index == (size_t)(-1)) {
            free_pcb(pcb);
            fclose(script);
            return NULL;
        }

        if (pcb->line_count == 0) {
            // do this on the first iteration only.
            pcb->line_base = index;
        }
        pcb->line_count++;

        // Update the page table
        if (index % 3 == 0) {
            pcb->pt[pointer] = index / 3;
            // index is monotonely increasing, so pointer++ is correct
            pointer++;
            pcb->pt[pointer] = -1; // the last entry of page table is -1
        }

        // Update the frame store for the first 2 pages
        if (pointer < 3) {
            //printf("index: %d\tpointer: %d\n", index, pointer);
            LRU_update(index);
            frame_store_size++;
        }
    }

    // We're done with the file, don't forget to close it!
    fclose(script);

    // duration should initially match line_count.
    pcb->duration = pcb->line_count;

    // Since each frame has 3 lines, allocate the next process starting at a multiple of 3
    fix_next_free_line();
    return pcb;
}

void free_pcb(struct PCB *pcb) {
    for (size_t ix = pcb->line_base; ix < pcb->line_base + pcb->line_count; ++ix) {
        free_line(ix);
    }
    // Free the process name, but only if it's not the empty string.
    // The empty name (for the shell input process) was not malloc'd.
    if (strcmp("", pcb->name)) {
        free(pcb->name);
    }
    free(pcb);
}
