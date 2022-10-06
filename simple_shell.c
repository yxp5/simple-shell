#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

// This is a simple shell that only support
// single piping and single output redirection
// It has built-in function: echo, cd, pwd, exit, jobs and fg
// Precondition: the simple shell expects at least one space between keywords and symbols



// maximum number of args in a command
#define LENGTH 80 

int running = 1;

int bg_process[LENGTH];
int bg_length = 0;
void removeBG();

// automatically detect finished child process in background and remove it from jobs
void handle_sigchld(int signal) {
    if (signal == SIGCHLD) {removeBG();}
}

void handle_sigint(int signal) {
    if (signal == SIGINT) {
        kill(getpid(), 0);
        printf("\nCurrent process killed\n");
    }
}

int getcommand(char *prompt, char *args[], int *background, int *output_redir, int *piping, int *arg2_start) {

    int length, flag, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    // check for invalid command
    if (length == 0) {
        return 0; 
    }
    // check for Ctrl + D
    else if (length == -1) {
        printf("\n");
        exit(0);  
    }

    // check if background is specified
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else {
        *background = 0;
    }
    
    // Clear args 
    memset(args, '\0', LENGTH);

    // Splitting the command and putting the tokens inside args[]
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) { 
                token[j] = '\0'; 
            } else if (token[j] == 62) {
                *output_redir = 1;
                *arg2_start = i;
                token[j] = '\0';
            } else if (token[j] == 124) {
                *piping = 1;
                *arg2_start = i;
                token[j] = '\0';
            }
        }
        if (strlen(token) > 0) {
            args[i++] = token;
        }
    }

    free(line);
    free(token);
    args[i] = NULL;
    return i;
}

void echo (char *args[], int count) {
    for (int i = 1; i < count; i++) {
        if (args[i] == NULL) {break;}
        printf("%s ", args[i]);
    }
    printf("\n");
}

void cd (char *args[], int count) {
    char buffer[50];
    if (count > 1) {
        chdir(args[1]);
    }
    printf("%s\n", getcwd(buffer, 50));
}

void pwd (void) {
    char buffer[50];
    printf("%s\n", getcwd(buffer, 50));
}

void exit_ (void) {
    for (int i = 0; i < bg_length; i++) {
        kill(bg_process[i], 0);
    }
    running = 0;
}

void jobs (void) {
    for (int i = 0; i < bg_length; i++) {
        if (bg_process[i] != '\0') {
            printf("[%d] Running       Process ID: %d\n", i+1, bg_process[i]);
        }
    }
}

void fg (char *args[], int count) {
    int index;
    if (count > 1) {
        index = atoi(args[1] - 1);
    } else {
        index = 0;
    }
    
    if (bg_process[index] == '\0') {
        return;
    }

    pid_t childpid = bg_process[index];
    waitpid(childpid, NULL, 0);
    bg_process[index] = '\0';
    bg_length--;
}

void putBG(int pid) {
    for (int i = 0; i < bg_length + 1; i++) {
        if (bg_process[i] == '\0') {
            bg_process[i] = pid;
            bg_length++;
            break;
        }
    }
}

void removeBG() {
    pid_t childpid;
    for (int i = 0; i < bg_length; i++) {
        if ((childpid = waitpid(bg_process[i], NULL, WNOHANG)) > 0) {
            bg_process[i] = '\0';
            bg_length--;
        }
    }
}

int execute (char *args[], int background, int count) {
    if (strcmp(args[0], "echo") == 0) {
        echo(args, count);
    } else if (strcmp(args[0], "cd") == 0) {
        cd(args, count);
    } else if (strcmp(args[0], "pwd") == 0) {
        pwd();
    } else if (strcmp(args[0], "jobs") == 0) {
        jobs();
    } else if (strcmp(args[0], "fg") == 0){
        fg(args, count);
    } else if (strcmp(args[0], "exit") == 0){
        exit_();
    } else {
        int pid = fork();
        if (pid < 0) {
            printf("Execution fork failed\n");
            exit(1);
        } else if (pid == 0) {
            if (execvp(args[0], args) < 0) {
                printf("Execution error (probably wrong inputs)\n");
                exit(1);
            }
            exit(0);
        } else {
            if (background == 0){ 
                waitpid(pid, NULL, 0);
            } else {
                putBG(pid);
            }
        }
    }
    return 0;
}

void doPipe (char *args1[], char *args2[], int background, int count) {
    int fd[2];
    pipe(fd);

    int pid1 = fork();
    if (pid1 < 0) {
        printf("Pipe fork 1 failed\n");
        exit(1);
    } else if (pid1 == 0) {      
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        execute(args1, background, count);
        exit(0);
    }
    
    int pid2 = fork();
    if (pid2 < 0) {
        printf("Pipe fork 2 failed\n");
        exit(1);
    } else if (pid2 == 0) {      
        dup2(fd[0], 0);
        close(fd[0]);
        close(fd[1]);
        execute(args2, background, count);
        exit(0);
    }

    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void doRedirection (char *args1[], char *args2[], int background, int count) {
    int fd = open(args2[0], O_WRONLY | O_CREAT | O_TRUNC, 0744); // full access to owner, read-only for others

    int pid1 = fork();
    if (pid1 < 0) {
        printf("Redirection fork failed\n");
        exit(1);
    } else if (pid1 == 0) {      
        dup2(fd, 1);
        close(fd);
        execute(args1, background, count);
        exit(0);
    }

    close(fd);
    waitpid(pid1, NULL, 0);
}

void argsSeparate (char *args[], char *args1[], char *args2[], int separate, int count) {
    int index;
    for (index = 0; index < separate; index++) {
        args1[index] = args[index];
    }
    args1[index] = NULL;

    int j = 0;
    for (index; index < count; index++) {
        args2[j++] = args[index];
    }
    args2[j] = NULL;
}

int main(void) { 
    printf("========================\n");
    printf("Lauching simple shell...\n");
    printf("Author: Yuxiang Pan\n");
    printf("Student ID: 261052613\n");
    printf("========================\n");

    char *args[LENGTH];
    char *args1[LENGTH];
    char *args2[LENGTH];
    int output_redir;   // flag for output redirection
    int piping;         // flag for piping
    int background;     // flag for running processes in the background
    int count;          // count of the arguments in the command
    int arg2_start;     // Useful since we are only doing single pipe or single output redirection

    for (int i = 0; i < LENGTH; i++) {bg_process[i] = '\0';}

    // Check for signals
    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR){ 
        printf("ERROR: could not bind signal handler for SIGCHLD\n");
        exit(1);
    }

    if (signal(SIGINT, handle_sigint) == SIG_ERR){ 
        printf("ERROR: could not bind signal handler for SIGINT\n");
        exit(1);
    }
    // Just Ignore Ctrl + Z, we don't need a handler
    signal(SIGTSTP, SIG_IGN);

    while(running) {
        // reset flags
        background = 0;    
        output_redir = 0;
        piping = 0;
        arg2_start = 0;

        if ((count = getcommand("\n>> ", args, &background, &output_redir, &piping, &arg2_start)) == 0) {
            printf("Invalid command\n");
            continue;
        }

        if (piping == 1 || output_redir == 1) {
            argsSeparate(args, args1, args2, arg2_start, count);
        }

        if (piping == 1) {
            doPipe(args1, args2, background, count);
            continue;
        } else if (output_redir == 1) {
            doRedirection(args1, args2, background, count);
            continue;
        } 
        
        if (execute(args, background, count) != 0) {
            printf("Execution failed\n");
        }
    }
}