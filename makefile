CC = gcc
CFLAGS = -Wall -Wextra -ggdb
LFLAGS = -lpthread -lrt
SRC  = sudoku-solver.c
OBJ  = sudoku-solver

all: $(OBJ)

$(OBJ):
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LFLAGS)

clean:
	rm -f *~ *.o $(OBJ)
