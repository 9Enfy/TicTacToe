NAMES= TriServer TriClient
CFLAGS=-Wall -std=gnu99 -g
INCLUDES=-I./inc

SRCS=src/TriServer.c src/TriClient.c src/SharedMemory
OBJS=$(SRCS:.c=.o)

all: TriServer TriClient 

TriServer : src/TriServer.o src/SharedMemory.o src/Segnali.o src/semaphore.o src/errExit.o
	@echo "Compilazione eseguibile TriServer"
	@$(CC) src/TriServer.o src/SharedMemory.o src/semaphore.o src/errExit.o src/Segnali.o -o TriServer

TriClient : src/TriClient.o src/SharedMemory.o src/semaphore.o src/Segnali.o src/errExit.o
	@echo "Compilazione eseguibile TriClient"
	@$(CC) src/TriClient.o src/SharedMemory.o src/semaphore.o src/errExit.o src/Segnali.o -o  TriClient 

src/SharedMemory.o : src/SharedMemory.c inc/SharedMemory.h
	@echo "Compilazione SharedMemory.o"
	@$(CC) $(CFLAGS) $(INCLUDES) -c src/SharedMemory.c -o src/SharedMemory.o

src/Segnali.o : src/segnali.c inc/Segnali.h
	@echo "Compilazione Segnali.o"
	@$(CC) $(CFLAGS) $(INCLUDES) -c src/segnali.c -o src/Segnali.o

src/semaphore.o : src/semaphore.c inc/semaphore.h 
	@echo "Compilazione semaphore.o"
	@$(CC) $(CFLAGS) $(INCLUDES) -c src/semaphore.c -o src/semaphore.o

src/errExit.o : src/errExit.c inc/errExit.h
	@echo "Compilazione errExit.o"
	@$(CC) $(CFLAGS) $(INCLUDES) -c src/errExit.c -o src/errExit.o 


src/TriServer.o : src/TriServer.c
	@echo "Compilazione TriServer.o"
	@$(CC) $(CFLAGS) $(INCLUDES) -c src/TriServer.c -o src/TriServer.o 

src/TriClient.o : src/TriClient.c
	@echo "Compilazione TriClient.o"
	@$(CC) $(CFLAGS) $(INCLUDES) -c src/TriClient.c -o src/TriClient.o

.PHONY: clean

clean:
	@rm -f src/*.o bin/* $(NAME)
	@rm TriClient TriServer
	@echo "Removed object files and executable..."

