#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "shellmemory.h"

#define true 1
#define false 0

// ---------------------
// Line memory for program lines; for part 2.
// ---------------------

// We know that program lines will be read, but not modified, until they are
// removed from the line memory. Therefore, we can provide the line directly
// to requests, rather than copying it. Freeing a program will free all of its
// lines.
// When a line is saved, it should be copied. This way, it can be saved from
// a stack-allocated buffer without fear of losing it when the buffer is freed.
struct program_line {
    int allocated; // for sanity-checking
    char *line;
};

struct program_line linememory[MEM_SIZE];

// We have two choices:
//  1. Offer an API that lets the client allocate one line at a time.
//  2. Offer an API that allocates whole programs at a time, and tracks their
//      locations for being freed later.
// (2) sounds harder, and it is, but that work needs to be done _somewhere_.
// The description suggests that rather than doing it here, we can do it in
// the PCB, so we'll do that. This will translate better to having page tables
// in PCBs in part 3 so it's probably the more sensible approach.

// Therefore, our API is to allocate one line at a time. Normally, we'd have
// to worry about fragmentation and contiguity, but, we're supposed to allocate
// everything before anything runs, and everything runs to completion before
// any more commands can execute, AND there's no recursive run/exec.
// All of that combined means we don't have to worry about fragmentation
// at all: between invocations of run/exec, the linememory will be EMPTY!
//
// Actually, there is one case of 'recursive' exec: the tests that look like
//   exec .... #
//   echo shell
//   exec ....
// The 'shell program' will recursively exec. The correct behavior in these
// cases is to enqueue new program(s), not run them to completion immediately.
// We're told we can assume that 1000 lines is enough to hold every program
// that is being tested simultaneously, so we won't worry about the condition
// that the linememory is empty once exec has been called with #.
// At that point, the assumption tells us that there is enough memory left.

size_t next_free_line = 0;
// So we simply record the next free index in a variable.
// We can't reset this whenever a PCB is freed, because we might be running
// with # and the shell might be about to allocate another program and that
// program might not fit in the first hole! So we provide a function to reset
// this, which should be called whenever `exec` is called and we're not in
// 'background mode.'
void reset_linememory_allocator() {
    next_free_line = 0;
    assert_linememory_is_empty();
}

// The comments above detail an important, nontrivial invariant, that the
// linememory is completely empty at certain times.
// For sanity checking, we provide an additional function to assert that the
// the linememory is empty.

void assert_linememory_is_empty() {
    for (size_t i = 0; i < MEM_SIZE; ++i) {
        assert(!linememory[i].allocated);
        assert(linememory[i].line == NULL);
    }
}

// note that init_linemem is not exposed from the header.
// We made init_mem call init_linemem, down below.
void init_linemem() {
    for (size_t i = 0; i < MEM_SIZE; ++i) {
        linememory[i].allocated = false;
        linememory[i].line = NULL;
    }
}

size_t allocate_line(const char *line) {
    if (next_free_line >= MEM_SIZE) {
        // out of memory!
        return (size_t)(-1);
    }
    size_t index = next_free_line++;
    assert(!linememory[index].allocated);

    linememory[index].allocated = true;
    // As described above, the allocator is only borrowing the string passed to it,
    // but linememory must own all strings it contains, so we need to copy the
    // string. (If you don't know what that means, see [Note: OBS].)
    linememory[index].line = strdup(line);
    return index;
}
// Since each frame has 3 lines, then always set next_free_line as a multiple of 3
void fix_next_free_line() {
    while (next_free_line % 3 != 0) {
        next_free_line++;
    }
}

// To free a line, we must deallocate it and adjust next_free.
void free_line(size_t index) {
    free(linememory[index].line);
    linememory[index].allocated = false;
    linememory[index].line = NULL;
}

// Return a const pointer to ensure the caller doesn't do something horrific,
// like try to free it.
const char *get_line(size_t index) {
    assert(linememory[index].allocated);
    return linememory[index].line;
}

// [Note: OBS]
// Thinking about memory in terms of "owners" and "borrowers" was
// popularized by Rust, and recently formalized by so-called
// Ownership and Borrowing Systems. They are a concrete way to ensure that our
// programs do not leak memory or access invalid memory, at the cost of
// potentially copying data sometimes that it is not necessary to.
// A small price to pay!
// We highly recommend looking into how this works and using it as
// a mental model when you program.

// --------------------- 
// Key-value memory for variables; from part 1.
// ---------------------

struct memory_struct {
    char *var;
    char *value;
};

struct memory_struct shellmemory[MEM_SIZE];

// Helper functions
int match(char *model, char *var) {
    int i, len = strlen(var), matchCount = 0;
    for (i = 0; i < len; i++) {
        if (model[i] == var[i]) matchCount++;
    }
    if (matchCount == len) {
        return 1;
    } else return 0;
}

// Shell memory functions

void mem_init(){
    int i;
    for (i = 0; i < MEM_SIZE; i++) {		
        // Include a character that's impossible to type so that
        // we don't mistake an empty entry for an entry named 'none'.
        shellmemory[i].var   = "none\1";
        shellmemory[i].value = "none";
    }

    init_linemem();
}

// Set key value pair
void mem_set_value(char *var_in, char *value_in) {
    int i;

    for (i = 0; i < MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, var_in) == 0) {
            free(shellmemory[i].value);
            shellmemory[i].value = strdup(value_in);
            return;
        } 
    }

    //Value does not exist, need to find a free spot.
    for (i = 0; i < MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, "none\1") == 0) {
            shellmemory[i].var   = strdup(var_in);
            shellmemory[i].value = strdup(value_in);
            return;
        } 
    }

    return;
}

//get value based on input key
char *mem_get_value(char *var_in) {
    int i;

    for (i = 0; i < MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, var_in) == 0){
            return strdup(shellmemory[i].value);
        } 
    }
    return NULL;
}