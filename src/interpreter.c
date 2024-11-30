#ifdef NDEBUG
#define debug(...)
#else
#define debug(...) fprintf(stderr, __VA_ARGS__)
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <ctype.h> // tolower, isdigit
#include <dirent.h> // scandir
#include <unistd.h> // chdir
#include <sys/stat.h> // mkdir
// for extra challenge:
#include <sys/types.h> // pid_t
#include <sys/wait.h> // waitpid

#include "pcb.h"
#include "queue.h"
#include "schedule_policy.h"
#include "shellmemory.h"
#include "shell.h"

#define true 1
#define false 0

#ifndef FRAMESIZE
#define FRAMESIZE 18
#endif

#ifndef VARMEMSIZE
#define VARMEMSIZE 10
#endif

#define MAX_ARGS_SIZE 7

int badcommand() {
    printf("Unknown Command\n");
    return 1;
}

int badcommandTooLong() {
    printf("Bad command: Too many tokens\n");
    return 2;
}

// For run command only
int badcommandFileDoesNotExist() {
    printf("Bad command: File not found\n");
    return 3;
}

int badcommandMkdir() {
    printf("Bad command: my_mkdir\n");
    return 4;
}

int badcommandCd() {
    printf("Bad command: my_cd\n");
    return 5;
}

int help();
int quit();
int set(char *var, char *value[], int value_size);
int print(char *var);
int echo(char *tok);
int ls();
int my_mkdir(char *name);
int touch(char *path);
int cd(char *path);
int run(char *script);
int my_exec(char *args[], int args_size);
int spawn(char *argv[], int args_size);

void runSchedule(struct queue *q, const struct schedule_policy *p);
int badcommandFileDoesNotExist();

// Interpret commands and their arguments
int interpreter(char *command_args[], int args_size) {
    int i;

    // these bits of debug output were very helpful for debugging
    // the changes we made to the parser!
    debug("#args: %d\n", args_size);
#ifndef NDEBUG
    for (size_t i = 0; i < args_size; ++i) {
        debug("  %ld: %s\n", i, command_args[i]);
    }
#endif

    if (args_size < 1) { 
        // this is only even possible because the spec says we shouldn't
        // just ignore blank lines. In a real implementation,
        // we would ignore them. (see parseInput in shell.c)
        return badcommand();
    }
    if (args_size > MAX_ARGS_SIZE) { // this is totally possible though
        return badcommandTooLong();
    }

    for (i = 0; i < args_size; i++) { // terminate args at newlines
        command_args[i][strcspn(command_args[i], "\r\n")] = 0;
    }

    if (strcmp(command_args[0], "help") == 0){
        //help
        if (args_size != 1) return badcommand();
        return help();
    
    } else if (strcmp(command_args[0], "quit") == 0) {
        //quit
        if (args_size != 1) return badcommand();
        return quit();

    } else if (strcmp(command_args[0], "set") == 0) {
        //set
        if (args_size < 3) return badcommand();
        if (args_size > 7) return badcommand();	
        return set(command_args[1], &command_args[2], args_size-2);
    
    } else if (strcmp(command_args[0], "print") == 0) {
        if (args_size != 2) return badcommand();
        return print(command_args[1]);

    } else if (strcmp(command_args[0], "echo") == 0) {
        if (args_size != 2) return badcommand();
        return echo(command_args[1]);

    } else if (strcmp(command_args[0], "my_ls") == 0) {
        if (args_size != 1) return badcommand();
        return ls();

    } else if (strcmp(command_args[0], "my_mkdir") == 0) {
        if (args_size != 2) return badcommand();
        return my_mkdir(command_args[1]);

    } else if (strcmp(command_args[0], "my_touch") == 0) {
        if (args_size != 2) return badcommand();
        return touch(command_args[1]);

    } else if (strcmp(command_args[0], "my_cd") == 0) {
        if (args_size != 2) return badcommand();
        return cd(command_args[1]);

    } else if (strcmp(command_args[0], "run") == 0) {
        if (args_size != 2) return badcommand();
        return run(command_args[1]);
    
    } else if (strcmp(command_args[0], "exec") == 0) {
        if (args_size < 2) return badcommand();
        return my_exec(&command_args[1], args_size - 1);

    } else if (strcmp(command_args[0], "spawn") == 0) {
        if (args_size < 2) return badcommand();
        return spawn(command_args+1, args_size-1);

    } else if (strcmp(command_args[0], "t") == 0) {
        chdir("../../A3-2024/test-cases");
        return 1;

    } else return badcommand();
}

int help() {

    // note the literal tab characters here for alignment
    char help_string[] = "COMMAND			DESCRIPTION\n \
help			Displays all the commands\n \
quit			Exits / terminates the shell with “Bye!”\n \
set VAR STRING		Assigns a value to shell memory\n \
print VAR		Displays the STRING assigned to VAR\n \
run SCRIPT.TXT		Executes the file SCRIPT.TXT\n ";
    printf("%s\n", help_string);
    return 0;
}

int quit() {
    printf("Bye!\n");
    exit(0);
}

int set(char *var, char *value[], int value_size) {
    // precondition: value_size in [1,5]
    char buffer[MAX_USER_INPUT];
    char *space = " ";

    strcpy(buffer, value[0]);
    for (size_t i = 1; i < value_size; i++) {
        strcat(buffer, space);
        strcat(buffer, value[i]);
    }

    mem_set_value(var, buffer);

    return 0;
}

int print(char *var) {
    char *value = mem_get_value(var);
    if (value) {
        printf("%s\n", value);
        free(value);
    } else {
        printf("Variable does not exist\n");
    }
    return 0;
}

int echo(char *tok) {
    int must_free = 0;
    // is it a var?
    if (tok[0] == '$') {
        tok++; // advance pointer, so that tok is now the stuff after '$'
        tok = mem_get_value(tok);
        if (tok == NULL) {
            tok = ""; // must use empty string, can't pass NULL to printf
        } else {
            must_free = 1;
        }
    }

    printf("%s\n", tok);

    // memory management technically optional for this assignment
    if (must_free) free(tok);

    return 0;
}

// We can hide dotfiles in ls using either the filter operand to scandir,
// or by checking the first character ourselves when we go to print
// the names. That would work, and is less code, but this is more robust.
// And this is also better since it won't allocate extra dirents.
int ls_filter(const struct dirent *d) {
    if (d->d_name[0] == '.') return 0;
    return 1;
}

int ls_compare_char(char a, char b) {
    // assumption: a,b are both either digits or letters.
    // If this is not true, the characters will be effectively compared
    // as ASCII when we do the lower_a - lower_b fallback.

    // if both are digits, compare them
    if (isdigit(a) && isdigit(b)) {
        return a - b;
    }
    // if only a is a digit, then b isn't, so a wins.
    if (isdigit(a)) {
        return -1;
    }
 
    // lowercase both letters so we can compare their alphabetic position.
    char lower_a = tolower(a), lower_b = tolower(b);
    if (lower_a == lower_b) {
        // a and b are the same letter, possibly in different cases.
        // If they are really the same letter, this returns 0.
        // Otherwise, it's negative if A was capital,
        // and positive if B is capital.
        return a - b;
    }

    // Otherwise, compare their alphabetic position by comparing
    // them at a known case.
    return lower_a - lower_b;
}

int ls_compare_str(const char *a, const char *b) {
    // a simple strcmp implementation that uses ls_compare_char.
    // We only check if *a is zero, since if *b is zero earlier,
    // it would've been unequal to *a at that time and we would return.
    // If *b is zero at the same point or later than *a, we'll exit the
    // loop and return the correct value with the last comparison.

    while (*a != '\0') {
        int d = ls_compare_char(*a, *b);
        if (d != 0) return d;
        a++, b++;
    }
    return ls_compare_char(*a, *b);
}

int ls_compare(const struct dirent **a, const struct dirent **b) {
    return ls_compare_str((*a)->d_name, (*b)->d_name);
}

int ls() {
    // straight out of the man page examples for scandir
    // alphasort uses strcoll instead of strcmp,
    // so we have to implement our own comparator to match the ls spec.
    // Note that the test cases weren't very picky about the specified order,
    // so if you just used alphasort with scandir, you should have passed.
    // This was intentional on our part.
    struct dirent **namelist;
    int n;

    n = scandir(".", &namelist, ls_filter, ls_compare);
    if (n == -1) {
        // something is catastrophically wrong, just give up.
        perror("my_ls couldn't scan the directory");
        return 0;
    }

    for (size_t i = 0; i < n; ++i) {
        printf("%s\n", namelist[i]->d_name);
        free(namelist[i]);
    }
    free(namelist);

    return 0;
}

int str_isalphanum(char *name) {
    for (char c = *name; c != '\0'; c = *++name) {
        if (!(isdigit(c) || isalpha(c))) return 0;
    }
    return 1;
}

int my_mkdir(char *name) {
    int must_free = 0;

    debug("my_mkdir: ->%s<-\n", name);

    if (name[0] == '$') {
        ++name;
        // lookup name
        name = mem_get_value(name);
        debug("  lookup: %s\n", name ? name : "(NULL)");
        if (name) {
            // name exists, should free whatever we got
            must_free = 1;
        }
    }
    if (!name || !str_isalphanum(name)) {
        // either name doesn't exist, or isn't valid, error.
        if (must_free) free(name);
        return badcommandMkdir();
    }
    // at this point name is definitely OK

    // 0777 means "777 in octal," aka 511. This value means
    // "give the new folder all permissions that we can."
    int result = mkdir(name, 0777);

    if (result) {
        // description doesn't specify what to do in this case,
        // (including if the directory already exists)
        // so we just give an error message on stderr and ignore it.
        perror("Something went wrong in my_mkdir");
    }

    if (must_free) free(name);
    return 0;
}

int touch(char *path) {
    // we're told we can assume this.
    assert(str_isalphanum(path));
    // if things go wrong, just ignore it.
    FILE *f = fopen(path, "a");
    fclose(f);
    return 0;
}

int cd(char *path) {
    // we're told we can assume this.
    assert(str_isalphanum(path));

    int result = chdir(path);
    if (result) {
        // chdir can fail for several reasons, but the only one we need
        // to handle here for the spec is the ENOENT reason,
        // aka Error NO ENTry -- the directory doesn't exist.
        // Since that's the only one we have to handle, we'll just assume
        // that that's what happened.
        // Alternatively, you can check if the directory exists
        // explicitly first using `stat`. However it is often better to
        // simply try to use a filesystem resource and then recover when
        // you can't, rather than trying to validate first. If you validate
        // first while two users are on the system, there's a race condition!
        return badcommandCd();
    }
    return 0;
}

void runSchedule(struct queue *q, const struct schedule_policy *policy) {
    struct PCB *next_pcb = policy->dequeue(q);
    while (next_pcb) {
        next_pcb = policy->run_pcb(next_pcb);
        if (next_pcb) policy->enqueue(q, next_pcb);
        next_pcb = policy->dequeue(q);
    }
}

struct PCB *run_pcb_to_completion(struct PCB *pcb) {
    while (pcb_has_next_instruction(pcb)) {
        size_t instr;
        // Check for page fault
        if (pcb->pointer < 2 && pcb->pt[pcb->pointer] == -1) {
            // Case: program smaller than 3 lines of code, finished running the only page already, then quite the while loop as the program is finished
            break;
        } else if (pcb->pointer < 2) {
            // Case: program that is still running the first 2 pages
            instr = pcb->pt[pcb->pointer] * 3 + pcb->residue;
            // Update residue by modulo 3
            if (pcb->residue == 2) {
                pcb->residue = 0;
                pcb->pointer++;
            } else {
                pcb->residue++;
            }
            // Still need to update pcb->pc
            pcb->pc++;
        } else if (pcb->pointer == 2 && !pcb->fault) {
            // Case: page fault but frame store is not full
            if (get_frame_store_size() < FRAMESIZE) {
                printf("Page fault!\n");
            }
            // Case: trying to access the third page but it should not be loaded, induce page fault
            printf("Page fault! Victim page contents:\n\n");
            for (int temp = 0; temp < 3; temp++) {
                printf("%s\n\n", get_line(pcb->pt[0] * 3 + temp));
            }
            printf("End of victim page contents.\n");
            // So the fault only run once
            pcb->fault = 1;
            instr = pcb_next_instruction(pcb);
        } else {
            instr = pcb_next_instruction(pcb);
        }
        parseInput(get_line(instr));
    }
    // don't free pcb if scheduled more than once
    if (pcb->sc == 0) {
        free_pcb(pcb);
    } else {
        // Reset fields for the next execution
        pcb->sc--;
        pcb->fault = 0;
        pcb->residue = 0;
        pcb->pointer = 0;
        pcb->pc = 0;
    }
    return NULL;
}

static size_t evict_count = 0;

void evict_count_update(size_t LR) {
    for (int i = 0; i < 3; i++) {
        if (in_frame_store(LR + i)) {
            evict_count++;
        } else {
            break;
        }
    }
}

static int toggle = 1;

struct PCB *run_pcb_for_n_steps(struct PCB *pcb, size_t n) {
    //printf("%d\t%d\n", pcb->pc, pcb->line_count);
    debug("run n steps: n is %ld\n", n);
    for (; n && pcb_has_next_instruction(pcb); --n) {
        //printf("program: %s\thas next instruction: %d\tpc: %d\n", pcb->name, pcb_has_next_instruction(pcb), pcb->pc);
        
        // Check for page fault
        if (pcb->pointer < 2 && pcb->pt[pcb->pointer] == -1) {
            // Case: program smaller than 3 lines of code, finished running the only page already, then quite the while loop as the program is finished
            break;
        }

        // Handling page fault
        size_t instr = pcb->pt[pcb->pointer] * 3 + pcb->residue;
        //printf("frame store presence: %d\tof instant: %d\n", in_frame_store(instr, get_frame_store()), instr);
        //print_frame_store();
        if (!in_frame_store(instr) && evict_count == 0 && toggle) {
            // Case: page fault, update frame store
            if (get_frame_store_size() < FRAMESIZE) {
                printf("Page fault!\n");
            }
            // Get the least recently used frame
            size_t LR = LR_frame() * 3;
            // Update evict count to print message
            evict_count_update(LR);
            // Evict and replace by demanded frame
            LRU_evict(LR, instr);
            // Print page fault message
            printf("Page fault! Victim page contents:\n\n");
            for (size_t i = 0; i < evict_count; i++) {
                printf("%s", get_line(LR+i));
            }
            printf("\nEnd of victim page contents.\n");
            evict_count = 0;
            if (get_frame_store_size() < FRAMESIZE) {
                toggle = 0;
            } else {
                break;
            }
        }

        // Case: no page fault, update residue by modulo 3
        if (pcb->residue == 2) {
            pcb->residue = 0;
            pcb->pointer++;
        } else {
            pcb->residue++;
        }
        // Still need to update pcb->pc
        pcb->pc++;
        // Update frame store in LRU way
        LRU_update(instr);
        //printf("program: %s\tmemory: %d\tpage number: %d\tresidue: %d\n", pcb->name, instr, pcb->pointer, pcb->residue);
        parseInput(get_line(instr));
    }
    debug("run n steps: looped to %ld\n", n);
    // The loop runs until either we've done n steps or the pcb is out of
    // instructions,  whichever happens first. But they might also happen
    // at the same time, in which case we should still clean up.
    // So check if there are more instructions, not the value of n.
    if (pcb_has_next_instruction(pcb)) {
        return pcb;
    } else {
        // don't free pcb if scheduled more than once
        if (pcb->sc == 1) {
            //free_pcb(pcb);
        } else {
            // Reset fields for the next execution
            pcb->sc--;
            pcb->fault = 0;
            pcb->residue = 0;
            pcb->pointer = 0;
            pcb->pc = 0;
        }
        return NULL;
    }
}

int run(char *script) {
    char *args[2] = {script, "FCFS"};
    return my_exec(args, 2);
}

// These are all global variables because they are not local to any particular
// call to exec. Once we go into background or into MT mode,
// the behavior of future calls is affected.
// Those calls also need to be able to see the existing queue.
// They could be defined `static` inside my_exec as well, but defining them
// outside allows the possibility for other functions to also care about
// the multithreaded/background state. For example, the `quit` function cares
// about the multithreaded state.
// Despite the top-level availability of these variables, global state is
// rather confusing to work with. So functions like runSchedule() above
// still take these values as arguments so they're easier to track.
static int multithreaded = false;
static int background = false;
static struct queue *q = NULL;
static const struct schedule_policy *policy = NULL;

int my_exec(char *args[], int args_size) {
    // Two inputs is the minimum. This should be checked above, but sanity:
    assert(args_size >= 2);
    // This exec call originates from a backgrounded shell script
    // if background is already true. We might update background later,
    // but we need to know if it was true when we started as well.
    int background_exec = background;
    // We don't know how many file names were passed, but we do know that
    // we have to look for specific strings at the end.
    // We technically have a choice here, because the assignment didn't
    // specify: is `exec prog1 RR MT` a single program, with RR and MT
    // policies? Or is it two programs, prog1 and RR, with the invalid
    // policy name MT?
    // Obviously, the intent of the description is that policy names,
    // as well as # and MT are "special" when in the right order and at the
    // end of the inputs even though they are all valid filenames.
    // So the only reasonable choice here is to treat it as a single
    // program, with RR and MT policies.
    // Of course, we don't test programs named # or MT, so this is a
    // philosophical point. But the syntax makes # and MT special only
    // when passed in the right order and as the last inputs.
    //
    // So we check for policy names, #, and MT by looking at the end
    // of the inputs, rather than trying to check for files first.


    // We check from the end, so we have to check in reverse order.
    // Look for MT first.
    if (strcmp(args[args_size-1], "MT") == 0) {
        // TODO: if multithreaded was previously false, call function
        // to initialize multithreaded scheduler here.
        multithreaded = true;
        // "remove" MT from the arguments by decrementing args size.
        args_size--;
    }
    // Now look for #.
    if (strcmp(args[args_size-1], "#") == 0) {
        background = true;
        args_size--; // ditto
    }
    // At this point, there must be at least two arguments;
    // a filename, and a policy. Similarly there are at most 4.
    // With the optional # and MT, it was harder
    // to tell that we were given well-formed input. But now we can check.
    // (It was safe to check for # and MT because we know there were at
    // least two arguments at the top, so decrementing args_size twice
    // was perfectly fine.)
    if (args_size < 2 || args_size > 4) {
        return badcommand();
    }
    // Now the last argument is the policy name.
    char *policy_name = args[args_size-1];
    args_size--;
    // Now the args,args_size array describes exactly the filenames.
    // We know the policy name now, so retrieve the actual policy.
    policy = get_policy(policy_name);
    if (!policy) {
        printf("Bad command: unknown scheduling policy\n");
        return 1;
    }

    if (!background_exec) {
        // In this case, we're a top-level exec call. We might be entering
        // background mode though. In either case, we should reset the
        // linememory allocator.
        reset_linememory_allocator();
        // We also need to allocate a queue to use.
        // We own this, so we have to be sure to free it later!
        assert(!q);
        q = alloc_queue();
    } else {
        // In this case, we're an exec call originating from the background.
        // Ensure that a queue exists.
        assert(q);
    }

    // Create a filename for each process, in order, and enqueue them.
    // We are allocating PCBs, but enqueue transfers ownership of the PCB
    // to the queue, so we're not responsible for freeing these.
    for (int n = 0; n < args_size; ++n) {
        struct PCB *pcb;
        if (program_already_scheduled(q, args[n])) {
            // Scheduling the same program
            pcb = get_program(q, args[n]);
        } else {
            pcb = create_process(args[n]);
            if (!pcb) {
                printf("Failed to create process\n");
                goto cleanup;
            }
        }
        policy->enqueue(q, pcb);
    }

    if (background && !background_exec) {
        // In this case, background mode is enabled but we're a top-level
        // exec call. Therefore, we're entering background mode.

        // add the rest of the input to a new program:
        struct PCB *pcb = create_process_from_FILE(stdin);
        if (!pcb) {
            printf("Failed to create STDIN process\n");
            goto cleanup;
        }
        // Ensure that this is scheduled first!
        policy->enqueue_ignoring_priority(q, pcb);
    }

    if (!background_exec) {
        // We should only start the scheduler if we are a top-level exec call.
        // If we are not top-level, it's already running!
        runSchedule(q, policy);
        // After the schedule completes, if we were given the # argument,
        // the exec should never 'return'. When it's done, so is the batch
        // mode script we are running. Therefore, if we get here without
        // invoking quit, we should quit.
        if (background) return quit();

        // If we are a top-level exec call,
        // we can now release the queue we allocated for scheduling.
top_level_cleanup:
        free_queue(q);
        q = NULL;
        policy = NULL;
    }

background_cleanup:
    return 0;

    // This is an unfortunately common error-handling pattern in C.
    // Without try-catch-finally as a language feature, there isn't a cleaner
    // way to handle certain kinds of errors.
    // This is essentially implementing a 'finally' block that guarantees
    // the queue allocated for top-level exec calls is freed.
    // Note that, if this is not a top-level exec call, we will not free
    // anything at all. That means this program might not behave as expected:
    //   exec P1 P2 #
    //   exec P3 P1
    //   quit
    // The schedule will end up containing P1, P2, _and_ P3, because the
    // background exec will refuse to allocate a second PCB for P1 but
    // will not de-allocate the PCB it already made for P3.
    // We are told we may handle corner cases however we like, so that's fine.
cleanup:
    if (background_exec) goto background_cleanup;
    else                 goto top_level_cleanup;
}

int spawn(char *argv[], int args_size) {
    // Need to make sure we have space to put a null pointer at the end
    // of args. From the definition of parseInput in shell, we might observe
    // that there's definitely space to just write one there... but that's
    // very bad separation of concerns. Instead we just take the time to
    // copy it here, into an array that we are certain, locally, has space.
    char *args[MAX_ARGS_SIZE] = {0};
    // first arg is the thing to execute, it's _also_ going to be args[0].
    char *file = argv[0];

    assert(args_size <= 6);

    int ix = 0;
    for ( ; ix < args_size; ++ix) {
        args[ix] = argv[ix];
    }
    args[ix] = NULL;

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
    }
    else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        // we are the child process
        execvp(file, args);
        // exec never returns, so we are only here if something went wrong.
        perror("Exec failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}
