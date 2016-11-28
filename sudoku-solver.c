#include <stdio.h>     //printf, file, getline
#include <pthread.h>   //multithreading
#include <unistd.h>
#include <sys/types.h> //types
#include <stdlib.h>    
#include <semaphore.h> //sem_wait, sem_post, sem_t
#include <string.h>    //strtok
#include <stdbool.h>   //bool, true, false

#define LENGTH(x)   (sizeof(x) / sizeof((x)[0])) //length of array
#define LIMIT       9 //max rows, max columns
#define MAXRETRIES  3 //max retries for each thread before exiting
#define INPUTFILE   "file/in.txt"
#define OUTPUTFILE  "file/out.txt"

void printBoard (int board[][LIMIT]);
void printBoardComparison (int problem[][LIMIT], int solution[][LIMIT]);
void *solveSection (void *cursorContext);

typedef struct boardCursor {
	int row, col;
} boardCursor;

sem_t mutex, rw_mutex;
int board[LIMIT][LIMIT];
int reference[LIMIT][LIMIT];

int main (void) {
	//initialize semaphores
	if (sem_init(&mutex, 0, 1) < 0 || sem_init(&rw_mutex, 0, 1) < 0) { // 0 = multithreaded
		fprintf(stderr, "ERROR: could not initialize &semaphore.\n");
		exit(0);
	}

	//read input file
	FILE *inputfile = fopen(INPUTFILE, "r");
	char *line;
	size_t len = 0;
	int row = 0;
	while (row < LIMIT && getline(&line, &len, inputfile) > 1) {
		int col = 0;
		//get each value from the input file
        line[strcspn(line, "\r\n")] = 0; //remove trailing newline char
		char *token = strtok(line, " ");
		while (col < LIMIT && token != NULL) {
			if (strcmp(token, "x") == 0)
				board[row][col] = -1;
			else
				board[row][col] = (int)strtol(token, NULL, 0);
			token = strtok(NULL, " ");
			col++;
		}
		row++;
	}
	fclose(inputfile);
	//clone the original board for visual comparisons
	memcpy(reference, board, sizeof(int)*LIMIT*LIMIT);

	//set up thread parameters
	pthread_t tid, tid2, tid3, tid4, tid5;
	pthread_attr_t attr, attr2, attr3, attr4, attr5;
	pthread_attr_init(&attr);
	pthread_attr_init(&attr2);
	pthread_attr_init(&attr3);
	pthread_attr_init(&attr4);
	pthread_attr_init(&attr5);

	boardCursor c1 = {8, 0};
	pthread_create(&tid, &attr, solveSection, &c1);
	boardCursor c2 = {7, 0};
	pthread_create(&tid2, &attr2, solveSection, &c2);
	boardCursor c3 = {6, 0};
	pthread_create(&tid3, &attr3, solveSection, &c3);
	boardCursor c4 = {5, 0};
	pthread_create(&tid4, &attr4, solveSection, &c4);
	boardCursor c5 = {4, 0};
	pthread_create(&tid5, &attr5, solveSection, &c5);

	//join all threads
	pthread_join(tid, NULL);
	pthread_join(tid2, NULL);
	pthread_join(tid3, NULL);
	pthread_join(tid4, NULL);
	pthread_join(tid5, NULL);

	//printBoard(board);
	printBoardComparison(reference, board);
	//clean up
	sem_destroy(&mutex);
	sem_destroy(&rw_mutex);
	return 0;
}

void *solveSection (void *cursorContext) {
	boardCursor *cursor = cursorContext;
	int row = cursor->row;
	int col = cursor->col;
	int solved = 0;
	//caclulate how many cells this section contains (diagonal cells)
	int toSolve;
	if (row > col)
		toSolve = LIMIT - row;
	else
		toSolve = LIMIT - col;
	//keep going until all needed cells are solved or num of retries too high
	int retries = 0;
	int prevSolved = 0;
	while (solved < toSolve && retries < MAXRETRIES) {
		//progress diagonally
		while (row < LIMIT && col < LIMIT) {
			sem_wait(&mutex);
			//solve cell
			if (board[row][col] == -1) {
				bool used[LIMIT]; //tracks which numbers can't be candidates
				int r, c;
				//eliminate options in rows and columns
				for (r=0; r < LIMIT; r++) {
					if (board[r][col] != -1)
						used[board[r][col]] = true;
				}
				for (c=0; c < LIMIT; c++) {
					if (board[row][c] != -1)
						used[board[row][c]] = true;
				}
				//eliminate options from 3x3
				r = (row / 3) * 3;
				c = (col / 3) * 3;
				int rMax = r + 2;
				int cMax = c + 2;
				for (; r < rMax; r++){
					for (; c < cMax; c++){
						if (board[r][c] != -1)
							used[board[r][c]] = true;
					}
				}
				//check if solution was found
				int i, temp, sum = 0;
				for (i=0; i < LIMIT; i++){
					sum += !used[i];
					if(!used[i])
						temp = i;
				}
				if (sum == 1){
					board[row][col] = temp;
					solved++;
				}
			}
			else
				solved++;
			sem_post(&mutex);
			row++;
			col++;
		}
		if (solved == prevSolved)
			retries++;
		else
			retries = 0;
		prevSolved = solved;
	}
	pthread_exit(0);
}

void printBoard (int board[][LIMIT]) {
	unsigned int r, c;
	for (r=0; r < LIMIT; r++) {
		for (c=0; c < LIMIT; c++) {
			if (board[r][c] == -1)
				printf("x ");
			else
				printf("%d ", board[r][c]);
		}
		printf("\n");
	}
}

void printBoardComparison (int problem[][LIMIT], int solution[][LIMIT]) {
	unsigned int r, c;
	for (r=0; r < LIMIT; r++) {
		for (c=0; c < LIMIT; c++) {
			if (problem[r][c] == -1)
				printf("x ");
			else
				printf("%d ", problem[r][c]);
		}
		if(r == LIMIT/2)
			printf("  ->  ");
		else
			printf("      ");
		for (c=0; c < LIMIT; c++) {
			if (solution[r][c] == -1)
				printf("x ");
			else
				printf("%d ", solution[r][c]);
		}
		printf("\n");
	}
}