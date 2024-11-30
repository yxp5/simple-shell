#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> // isspace
#include <string.h>
#include <unistd.h> // isatty
#include "shell.h"
#include "interpreter.h"
#include "shellmemory.h"

#ifndef FRAMESIZE
#define FRAMESIZE 18
#endif

#ifndef VARMEMSIZE
#define VARMEMSIZE 10
#endif

// Start of everything
int main(int argc, char *argv[]) {
    // Print the frame store and variable store size
    printf("Frame Store Size = %d; ", FRAMESIZE);
    printf("Variable Store Size = %d\n", VARMEMSIZE);

    char prompt = '$';  				// Shell prompt
    char userInput[MAX_USER_INPUT];		// user's input stored here
    // batch_mode is true when a file was given.
    int batch_mode = !isatty(STDIN_FILENO);
    int errorCode = 0;					// zero means no error, default

    //init user input
    for (int i = 0; i < MAX_USER_INPUT; i++) {
        userInput[i] = '\0';
    }
    
    //init shell memory
    mem_init();
    while(1) {
        if (!batch_mode) {
            printf("%c ", prompt);
        }
        fgets(userInput, MAX_USER_INPUT-1, stdin);
        errorCode = parseInput(userInput);
        if (errorCode == -1) exit(99); // ignore all other errors

        if (feof(stdin)) {
            // Remove returning to interactive mode
            return 0;
            //stdin = fopen("/dev/tty", "r");
            //batch_mode = 0;
        }

        memset(userInput, 0, sizeof(userInput));
    }

    return 0;
}

int wordEnding(char c) {
    // You may want to add ';' to this at some point,
    // or you may want to find a different way to implement chains.
    return c == '\0' || c == '\n' || isspace(c) || c == ';';
}

int parseInput(const char inp[]) {
    char tmp[200], *words[100];                            
    int ix = 0, w = 0;
    int wordlen;
    int errorCode = 0;

    // This function probably isn't the best place to handle chains.
    // That is, if we really wanted to implement relatively complex
    // syntax like that of bash, we should really just tokenize everything,
    // send the ';' as a separate word to the interpreter, and let the
    // interpreter sort it out later.
    // But for this simple shell, the interpreter's job is really only to be
    // command dispatch, and this function is really acting as a complete
    // parser rather than just a tokenizer. So we'll handle it here.

    while (inp[ix] != '\n' && inp[ix] != '\0' && ix < 1000) {
        // skip white spaces
        for ( ; isspace(inp[ix]) && inp[ix] != '\n' && ix < 1000; ix++);

        // If the next character is a semicolon,
        // we should run what we have so far.
        if (inp[ix] == ';') break;

        // extract a word
        for (wordlen = 0; !wordEnding(inp[ix]) && ix < 1000; ix++, wordlen++) {
            tmp[wordlen] = inp[ix];
        }

        if (wordlen > 0) {
            tmp[wordlen] = '\0';
            words[w] = strdup(tmp);
            w++;
            if (inp[ix] == '\0') break;
        } else {
            break;
        }
    }
    // In a real shell, we'd really want to ignore lines that don't
    // contain any instructions. But the spec (particularly the blanklines test)
    // insists that we instead print an error message.
    // The "fix" to ignore those lines would be to uncomment this if.
    //if (w > 0) {
        errorCode = interpreter(words, w);
        for (size_t i = 0; i < w; ++i) {
            free(words[i]);
        }
    //}
    if (inp[ix] == ';') {
        // handle the next command in the chain by literally recursing
        // the parser. We could equivalently wrap all of the work above
        // in a while loop, but this makes it clearer what's going on.
        // Additionally, a modern compiler is more than smart enough to
        // turn this into a loop for us! Try adding -O2 to the CFLAGS in
        // the Makefile and then read the assembly we get.
        return parseInput(&inp[ix+1]);
    }
    return errorCode;
}
