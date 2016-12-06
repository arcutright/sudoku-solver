#include <stdio.h>     //printf, file, getline
#include <pthread.h>   //multithreading
#include <unistd.h>
#include <sys/types.h> //types
#include <stdlib.h>    
#include <semaphore.h> //sem_wait, sem_post, sem_t
#include <string.h>    //strtok
#include <stdbool.h>   //bool, true, false
#include <time.h>      //clock_t

#define LIMIT       9 //max rows, max columns
#define MAXRETRIES  50 //max retries for each thread before exiting
#define INPUTFILE   "file/in.txt"
#define OUTPUTFILE  "file/out.txt"

void printBoard (int board[][LIMIT]);
void printBoardComparison (int problem[][LIMIT], int solution[][LIMIT]);
void *solveSectionDiagonally (void *cursorContext);
void *solveSectionHorizontally (void *cursorContext);
bool isFinished(int board[][LIMIT]);
bool isCorrect(int board[][LIMIT]);
int percentSolved(int board[][LIMIT]);
int trySolve(int row, int col, int solved);

typedef struct boardCursor {
	int row, col;
} boardCursor;

int board[LIMIT][LIMIT];
int reference[LIMIT][LIMIT];
sem_t mutex;

int main (void) {
	//initialize semaphores
	if (sem_init(&mutex, 0, 1) < 0 /*|| sem_init(&rw_mutex, 0, 1) < 0*/) { // 0 = multithreaded
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

	//thread variables
	pthread_t tids[17];
	pthread_attr_t attrs[17];
	boardCursor curs[17];
	int i;
	clock_t begin, end;
	
	//1 thread, horizontal
	int tries = 0;
	int percentSolvd = 0;
	begin = clock();
	while(percentSolvd < 100 && tries < 1000) {
		for(i=0; i < 9; i++){
			curs[i].row = 8 - i;
			curs[i].col = 0;
			solveSectionHorizontally(&curs[i]);
		}
		percentSolvd = percentSolved(board);
		tries++;
	}
	end = clock();
	printf("correct solution: %s (%d tries)\n", isCorrect(board) ? "true" : "false", tries);
	printf("time taken: %.02lfms (horizontal solve, 1 thread)\n", (double)(end - begin) / CLOCKS_PER_SEC * 1000);

	//1 thread, diagonal
	tries = 0;
	percentSolvd = 0;
	begin = clock();
	while(percentSolvd < 100 && tries < 1000) {
		for(i=0; i < 17; i++){
			if(i < 9){
				curs[i].row = 8 - i;
				curs[i].col = 0;
			}
			else {
				curs[i].row = 0;
				curs[i].col = i - 8;
			}
			solveSectionDiagonally(&curs[i]);
		}
		percentSolvd = percentSolved(board);
		tries++;
	}
	end = clock();
	printf("correct solution: %s (%d tries)\n", isCorrect(board) ? "true" : "false", tries);
	printf("time taken: %.02lfms (diagonal solve, 1 thread)\n", (double)(end - begin) / CLOCKS_PER_SEC * 1000);

	//10 threads, horizontal (one per row)
	begin = clock();
	for(i=0; i < 10; i++){
		pthread_attr_init(&attrs[i]);
		curs[i].row = 9 - i;
		curs[i].col = 0;
		pthread_create(&tids[i], &attrs[i], solveSectionHorizontally, &curs[i]);
	}
	for(i=0; i < 10; i++)
		pthread_join(tids[i], NULL);
	end = clock();
	printf("correct solution: %s\n", isCorrect(board) ? "true" : "false");
	printf("time taken: %.02lfms (horizontal solve, 10 threads)\n", (double)(end - begin) / CLOCKS_PER_SEC * 1000);

	//17 threads, diagonal (one per diagonal)
	begin = clock();
	for(i=0; i < 17; i++){
		pthread_attr_init(&attrs[i]);
		if(i < 9){
			curs[i].row = 8 - i;
			curs[i].col = 0;
		}
		else{
			curs[i].row = 0;
			curs[i].col = i - 8;
		}
		pthread_create(&tids[i], &attrs[i], solveSectionDiagonally, &curs[i]);
	}
	for(i=0; i < 17; i++)
		pthread_join(tids[i], NULL);
	end = clock();
	printf("correct solution: %s\n", isCorrect(board) ? "true" : "false");
	printf("time taken: %.02lfms (diagonal solve, 17 threads)\n", (double)(end - begin) / CLOCKS_PER_SEC * 1000);

	//display problem and solution
	printBoardComparison(reference, board);
	//clean up
	sem_destroy(&mutex);
	return 0;
}

void *solveSectionDiagonally (void *cursorContext) {
	boardCursor *cursor = cursorContext;
	int row = cursor->row;
	int col = cursor->col;

	int solved; //num of non-empty cells in diagonal
	//caclulate how many cells this section contains (diagonal cells)
	int toSolve;
	if (row > col)
		toSolve = LIMIT - row;
	else
		toSolve = LIMIT - col;
	//keep going until all needed cells are solved or num of retries too high
	long long retries = 0;
	int prevSolved = 0;
	while (solved < toSolve && retries < MAXRETRIES) {
		solved = 0;
		//progress diagonally
		while (row < LIMIT && col < LIMIT) {
			//solve cell
			if (board[row][col] == -1)
				solved = trySolve(row, col, solved);
			else
				solved++;
			row++;
			col++;
		}
		if (solved == prevSolved)
			retries++;
		else
			retries = 0;
		prevSolved = solved;
	}
	return 0;
	pthread_exit(0);
}

void *solveSectionHorizontally (void *cursorContext) {
	boardCursor *cursor = cursorContext;
	int row = cursor->row;
	int col = cursor->col;

	int solved = 0; //number of non-empty cells in row
	int toSolve = LIMIT; //number of cells per row
	//keep going until all needed cells are solved or num of retries too high
	long long retries = 0;
	int prevSolved = 0;

	while (solved < toSolve && retries < MAXRETRIES) {
		//progress horizontally
		for (col=0; col < LIMIT; col++) {
			//solve cell
			if (board[row][col] == -1)
				solved = trySolve(row, col, solved);
			else
				solved++;
		}
		if (solved == prevSolved)
			retries++;
		else
			retries = 0;
		prevSolved = solved;
	}
	return 0;
	pthread_exit(0);
}

int trySolve(int row, int col, int solved){
	bool used[10] = {0,0,0,0,0,0,0,0,0,0}; //tracks which numbers can't be candidates in [1]..[9]
	used[0] = true; //0 is never an option in sudoku
	int r, c;
	//eliminate options from column
	for (r=0; r < LIMIT; r++) {
		if (board[r][col] != -1)
			used[board[r][col]] = true;
	}
	//eliminate options from row
	for (c=0; c < LIMIT; c++) {
		if (board[row][c] != -1)
			used[board[row][c]] = true;
	}
	//check if solution was found
	int i, last, sum = 0;
	for (i=1; i < 10; i++){
		sum += !used[i];
		if(!used[i])
			last = i;
	}
	if (sum == 1){
		board[row][col] = last;
		solved++;
	} 
	else {
		sem_wait(&mutex);
		//eliminate options from 3x3
		r = (row / 3) * 3;
		c = (col / 3) * 3;
		int rMax = r + 2;
		int cMax = c + 2;
		for (r = (row / 3) * 3; r <= rMax; r++){
			for (c = (col / 3) * 3; c <= cMax; c++){
				if (board[r][c] != -1)
					used[board[r][c]] = true;
			}
		}
		sem_post(&mutex);
		//check if solution was found
		sum = 0;
		for (i=1; i < 10; i++){
			sum += !used[i];
			if(!used[i])
				last = i;
		}
		if (sum == 1){
			board[row][col] = last;
			solved++;
		}
	}
	return solved;
}

bool isSolved(int board[][LIMIT]){
	int r, c;
	for(r=0; r < 9; r++){
		for(c=0; c < LIMIT; c++){
			if(board[r][c] == -1)
				return false;
		}
	}
	return true;
}

bool isCorrect(int board[][LIMIT]){
	int r, c;
	int validsum = 0, sum = 0;
	for(r=1; r <= LIMIT; r++){
		validsum += r;
	}
	for(r=0; r < LIMIT; r++){
		sum = 0;
		for(c=0; c < LIMIT; c++){
			sum += board[r][c];
		}
		if(sum != validsum)
			return false;
	}
	return true;	
}

int percentSolved(int board[][LIMIT]){
	int r, c;
	float percent = 0;
	for(r=0; r < 9; r++){
		for(c=0; c < LIMIT; c++){
			if(board[r][c] != -1)
				percent += 1.f/(LIMIT*LIMIT);
		}
	}
	return (int)(percent*100 + 0.5);
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