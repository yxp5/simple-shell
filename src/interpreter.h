#pragma once
int interpreter(char *command_args[], int args_size);
int help();

// Run the given PCB to completion, then clean it up and return NULL.
// Suitable implementation of schedule_policy::run_pcb.
struct PCB *run_pcb_to_completion(struct PCB *pcb);
// Run the given PCB for the given number of steps.
// If it has remaining instructions, return it.
// Otherwise, clean it up and return NULL.
// Partial applications of n make this suitable for schedule_policy::run_pcb.
struct PCB *run_pcb_for_n_steps(struct PCB *pcb, size_t n);
