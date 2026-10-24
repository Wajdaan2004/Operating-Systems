# Do not edit the contents of this file.
CC = gcc
CFLAGS = -Werror -Wall -g -std=c11
LDFLAGS =
TARGET = assignment_average process_dispatcher

all: $(TARGET)

assignment_average: assignment_average.c
	$(CC) $(CFLAGS) -o assignment_average assignment_average.c $(LDFLAGS)

process_dispatcher: process_dispatcher.c
	$(CC) $(CFLAGS) -o process_dispatcher process_dispatcher.c $(LDFLAGS)

runq1: assignment_average
	./assignment_average sample_in_grades.txt

runq2: process_dispatcher
	./process_dispatcher sample_in_dispatcher.txt

clean:
	rm -rf process_dispatcher assignment_average *.exe *.dSYM output.txt output_ref.txt *.out
