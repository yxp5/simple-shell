#pragma once
#include <stddef.h>
#include <stdio.h> // FILE

typedef size_t pid;

int get_frame_store_size();
int* get_frame_store();
int in_frame_store(int instr);
int LR_frame();
void LRU_update(int instr);
void LRU_evict(int evict, int instr);
void print_frame_store();
// A process info struct.
struct PCB {
    pid pid;
    // Store the process name, which we can use to detect that multiple
    // processes with the same name are being scheduled.
    // We set it to the empty string if the process name is not known,
    // which should only happen when the process is the 'shell input'
    // background process.
    // Note: on Linux, knowing the filenames is not enough to guarantee
    // that the files are actually different. Surprise!
    // There are a few reasons for this, e.g. `a/b` is the same file as
    // `a/../a/b` but also so-called "hardlinks." There's a way to detect
    // if two open files have the same "inode number," which is what we
    // would want to do in a real OS.
    // But for our purposes, we're going to stick to doing comparison by name.
    char *name;
    // Base+count form a base-bounds pair for the "memory allocated"
    // to this process. Seeing it this way helps see how we can replace
    // it with pseudo-paging later. Spoilers!
    size_t line_base;
    size_t line_count;

    // This field is used for SJF and aging, and should initially have
    // the same value as line_count.
    size_t duration;

    // pc is the number of the instruction next to execute.
    // For example, it is initially 0, **regardless** of the value of
    // line_base. (Think of it as the "virtual address" of the next insn.)
    size_t pc;

    // As far as we're concerned, the only purpose of PCBs is to manage
    // scheduling; our "multiprocessing" structure simply isn't complicated
    // enough to warrant a factored-out PCB struct that can be used
    // in many different contexts. So we entangle it with the queue by just
    // including the next pointer here directly. The alternative would be
    // to invent a queue structure that has a pointer to a PCB,
    // and manage it separately.
    // If this PCB is the tail of the queue, next is NULL.
    struct PCB *next;

    // Same PCB active scheduled count
    size_t sc;
    // Page table array
    size_t pt[1000];
    // Page table pointers to help code loading
    size_t residue;
    size_t pointer;
    size_t fault;
};

// Returns non-zero iff there are more instructions to execute.
int pcb_has_next_instruction(struct PCB *pcb);
// Get the shellmemory index of the next instruction, and increment pc.
size_t pcb_next_instruction(struct PCB *pcb);
// Create a new process from the given filename:
//   1. Allocates a new PCB
//   2. Loads the code from the script file into shellmemory
//   3. Does NOT enqueue the PCB to any scheduling queue (next is NULL)
struct PCB *create_process(const char *filename);
// Like create_process, but takes a FILE* directly.
// Ownership of the FILE* is taken and it will be closed.
struct PCB *create_process_from_FILE(FILE *f);
// Cleanup a process:
//   1. Free all shellmemory used by the process code
//   2. Free the PCB
void free_pcb(struct PCB *pcb);


