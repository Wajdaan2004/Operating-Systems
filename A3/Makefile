# Do not edit the contents of this file.
CC = gcc
CFLAGS = -Wall -g -std=gnu99
LDFLAGS = -lrt -lpthread -lm
TARGET = pi sudoku
OBJFILES = pi.o sudoku.o
all: $(TARGET)

sudoku: sudoku.c
	$(CC) $(CFLAGS) -o sudoku sudoku.c $(LDFLAGS)
pi: pi.c
	$(CC) $(CFLAGS) -o pi pi.c  $(LDFLAGS)
runq1: pi
	./pi 5000000 5
runq1e1: pi
	./pi 
runq1e2: pi
	./pi 500000 1
runq1e3: pi
	./pi 5000000 1
runq2: sudoku
	./sudoku sample_in_sudoku.txt
runq2e1: sudoku
	./sudoku
runq2e2: sudoku
	./sudoku file_does_not_exist.txt
clean:
	rm -rf $(OBJFILES) $(TARGET) *~ 
	rm -rf *.out