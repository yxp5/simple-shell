#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "pcb.h"
#include "queue.h"

struct queue {
    struct PCB *head;

    // Why no tail? Simply put; there's only ever 4 items in our queue.
    // Dereferencing 4 pointers is not so slow.
    // On the other hand, properly implementing the bookkeeping for a tail
    // pointer is headache-inducing!
    //struct PCB *tail;

    // pthread_mutex_t lock;
};

// INVARIANT:
// If a PCB is not currently on the queue, its next pointer is NULL.

struct queue *alloc_queue() {
    struct queue *q = malloc(sizeof(struct queue));
    q->head = NULL;
    return q;
}

void free_queue(struct queue *q) {
    // Free all PCBs in the queue as well!
    // This might be relevant if we discover an error
    // while creating the schedule, e.g. can't open a file or
    // two scripts have the same name.
    struct PCB *p = q->head;
    while (p) {
        struct PCB *next = p->next;
        free_pcb(p);
        p = next;
    }
    free(q);
}

int program_already_scheduled(struct queue *q, char *name) {
    struct PCB *p = q->head;
    while (p) {
        if (strcmp(p->name, name) == 0) return 1;
        p = p->next;
    }
    return 0;
}

struct PCB *get_program(struct queue *q, char *name) {
    struct PCB *p = q->head;
    while (p) {
        if (strcmp(p->name, name) == 0) {
            p->sc++;
            return p;
        }
        p = p->next;
    }
}

void enqueue_ignoring_priority(struct queue *q, struct PCB *pcb) {
    pcb->next = q->head;
    q->head = pcb;
}

void enqueue_fcfs(struct queue *q, struct PCB *pcb) {
    // sanity check: some dequeue operation didn't do its job if this isn't NULL.
    assert(pcb->next == NULL);
    struct PCB *p = q->head;

    if (!p) {
        q->head = pcb;
        return;
    }

    // otherwise p is a real PCB.
    // Until it's the tail, step down the list.
    while (p->next) {
        p = p->next;
    }

    // Now p is the current tail of the queue. Append pcb.
    p->next = pcb;
}

void enqueue_sjf(struct queue *q, struct PCB *pcb) {
    size_t dur = pcb->duration;

    struct PCB *p = q->head;
    // if the queue was empty, or the head is a longer job than pcb,
    // pcb is just the new head.
    if (!p || p->duration > dur) {
        pcb->next = p;
        q->head = pcb;
        return;
    }

    // Otherwise, the queue was not empty. As long as the next item exists,
    // check it. If its duration is longer than dur, pcb goes between
    // p and p->next. Otherwise, step and try again.
    while (p->next) {
        if (p->next->duration > dur) {
            pcb->next = p->next;
            p->next = pcb;
            return;
        }
        p = p->next;
    }
    // If we make it to the end, then this is the longest job.
    p->next = pcb;
}

void enqueue_aging(struct queue *q, struct PCB *pcb) {
    // There's a small bit of complexity here:
    // The behavior of AGING enqueue is slightly different during the initial
    // enqueues and the re-queues that occur after each time slice.
    // During the initial enqueue, ties at the head should be broken like any
    // other ties: by walking back until there's no longer a tie.
    // This is checked, for example, by T_AGING4. We must queue the
    // equal-length input programs in the order they were given.
    // If we _always_ break ties at the head like any other tie, then we will
    // sometimes preempt programs when we shouldn't. If we always break ties
    // at the head by keeping the current head, we won't do the initial setup
    // for T_AGING4 correctly.
    // The "best" solution is probably to have a different enqueue function
    // for the initial setup and for the re-queueing that happens later.
    // But we can instead do a little hack: we know that programs that are
    // scheduled will **always** run at least one step.
    // Therefore, we can tell whether or not we are in the initial case
    // by checking if pcb->pc is 0.
    if (q->head && q->head->duration == pcb->duration && pcb->pc) {
        enqueue_ignoring_priority(q, pcb);
    } else {
        enqueue_sjf(q, pcb);
    }
}


struct PCB *dequeue_typical(struct queue *q) {
    if (q->head == NULL) {
        return NULL;
    }

    // q -> head -> next
    struct PCB *head = q->head;
    // q -> next
    q->head = head->next;

    head->next = NULL;
    return head;
}

void debug_with_age(struct queue *q) {
    struct PCB *pcb = q->head;
    printf("q");
    while (pcb) {
        printf(" -> %ld %s", pcb->duration, pcb->name);
        pcb = pcb->next;
    }
    printf("\n");
}

struct PCB *dequeue_aging(struct queue *q) {
    //debug_with_age(q);
    struct PCB *r = dequeue_typical(q);

    struct PCB *p = q->head;
    while (p) {
        if (p->duration > 0) {
            p->duration--;
        }
        p = p->next;
    }

    return r;
}
