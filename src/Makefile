CC=gcc
#CFLAGS=-g -O0 #-DNDEBUG
CFLAGS=-DNDEBUG

# Allow passing framesize and varmemsize as parameters
ifdef framesize
    CFLAGS += -DFRAMESIZE=$(framesize)
endif

ifdef varmemsize
    CFLAGS += -DVARMEMSIZE=$(varmemsize)
endif

mysh: shell.c interpreter.c shellmemory.c
	$(CC) $(CFLAGS) -c shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) -o mysh shell.o interpreter.o shellmemory.o pcb.o queue.o schedule_policy.o

clean: 
	rm mysh; rm *.o
