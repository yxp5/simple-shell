#pragma once
#include "queue.h"

// A collection of functions that collectively implement the logic of a
// scheduling policy. Requires a separate client to drive the action.
struct schedule_policy {
    // Run the given PCB. Return the given PCB if it should be re-scheduled,
    // otherwise clean up the PCB and return NULL.
    struct PCB *(*run_pcb)(struct PCB*);
    // Enqueue the given PCB. If this policy is a priority queue (e.g. SJF),
    // the PCB may not end up at the tail of the queue.
    void (*enqueue)(struct queue*, struct PCB*);
    // Enqueue the given PCB. It *will* be the head of the queue.
    void (*enqueue_ignoring_priority)(struct queue*, struct PCB*);
    // Retrieve (and remove) the highest-priority PCB from the queue.
    // If an operation such as aging is to be performed on other members,
    // it is done at this time.
    struct PCB *(*dequeue)(struct queue*);
};

const struct schedule_policy *get_policy(const char *policy_name);

// Notes on particular policies:
//
// SJF:
//  Ties are broken via FCFS.
// Aging:
//  Except when first scheduling a PCB, ties at the head are broken by
//  enqueuing directly to the head.
//  This makes it so that a dequeue=>enqueue aging sequence leaves the original
//  head in place if nothing has a _lower_ duration during the enqueue.
//
//  Otherwise (tie not at the head, or during first scheduling),
//  we break ties with FCFS like SJF.
