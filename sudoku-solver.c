#include <stdio.h>     //printf, file, getline
#include <pthread.h>   //multithreading
#include <unistd.h>
#include <sys/types.h> //types
#include <stdlib.h>    
#include <semaphore.h> //sem_wait, sem_post, sem_t
#include <string.h>    //strtok
#include <stdbool.h>   //bool, true, false
#include <time.h>      //clock_t

//constants
#define INPUTFILE   "file/in.txt"
#define OUTPUTFILE  "file/out.txt"
#define LIMIT       9 //max rows, max columns
#define MAXRETRIES_SINGLETHREADED   1  //max retries per row/diagonal w/ 1 thread
#define MAXRETRIES_MULTITHREADED  500 //max retries per thread before exiting
#define RETRY_WAIT_NS              10
/* note: MAXRETRIES is high to try to probabilistically get several full
   runs at solving the board instead of guaranteeing it. To get a full
   guarantee, I would be effectively making several threads act as one
   also, there is a RETRY_WAIT_NS pause between each retry */

//new types
typedef struct board_t {
	int board[LIMIT][LIMIT];
} board_t;
typedef struct thread_container_t {
	int row, col;
	int max_retries;
	board_t *board_wrapper_ptr;
} thread_container_t;

//i/o functions
board_t readBoardFromFile(char *filepath);
void writeBoardToFile(int board[][LIMIT], char *filepath);
void printBoard(int board[][LIMIT]);
void printBoardComparison(int problem[][LIMIT], int solution[][LIMIT]);
//solving functions
void solveBoardInManyWays(board_t *wrapper);
void *solveSectionDiagonally(void *thread_container);
void *solveSectionHorizontally(void *thread_container);
int trySolve(int board[][LIMIT], int row, int col, int solved);
void sleep_ns(int ns);
//validation functions
bool isCorrect(int board[][LIMIT]);
int percentSolved(int board[][LIMIT]);

//global vars
int reference[LIMIT][LIMIT];
sem_t mutex;

int main(void) {
	//initialize semaphores
	if (sem_init(&mutex, 0, 1) < 0) { // 0 = multithreaded
		fprintf(stderr, "ERROR: could not initialize &semaphore.\n");
		exit(0);
	}
	//get problem board from input file, make a clone
	board_t wrapper = readBoardFromFile(INPUTFILE);
	memcpy(reference, wrapper.board, sizeof(int)*LIMIT*LIMIT);
	
	//solve board in-place
	solveBoardInManyWays(&wrapper);
	
	//dump solution
	printBoardComparison(reference, wrapper.board);
	writeBoardToFile(wrapper.board, OUTPUTFILE);

	//clean up
	sem_destroy(&mutex);
	return 0;
}

void solveBoardInManyWays(board_t *wrapper_ptr) {
	//thread variables
	pthread_t tids[17];
	pthread_attr_t attrs[17];
	thread_container_t curs[17];
	int i;
	clock_t begin, end;
	//macro for board, c version of 'int board[][] = wrapper.board'
	int (*board)[LIMIT] = wrapper_ptr->board;
	
	//1 thread, horizontal
	int tries = 0;
	int percentSolvd = 0;
	begin = clock();
	while(percentSolvd < 100 && tries < 1000) { //1000 tries to get it right
		for(i=0; i < 9; i++){
			curs[i].row = 8 - i;
			curs[i].col = 0;
			curs[i].board_wrapper_ptr = wrapper_ptr;
			curs[i].max_retries = MAXRETRIES_SINGLETHREADED;
			solveSectionHorizontally(&curs[i]);
			percentSolvd = percentSolved(board);
			if(percentSolvd == 100)
				break;
		}
		tries++;
	}
	end = clock();
	printf("time taken: %.02lfms (horizontal solve, 1 thread) correct solution: %s\n", (double)(end - begin) / CLOCKS_PER_SEC*1000,
		isCorrect(board) ? "true" : "false");
	//reset board for next solve
	memcpy(board, reference, sizeof(int)*LIMIT*LIMIT);

	//1 thread, diagonal
	tries = 0;
	percentSolvd = 0;
	begin = clock();
	while(percentSolvd < 100 && tries < 1000) { //1000 tries to get it right
		for(i=0; i < 17; i++){
			if(i < 9){
				curs[i].row = 8 - i;
				curs[i].col = 0;
			}
			else {
				curs[i].row = 0;
				curs[i].col = i - 8;
			}
			curs[i].board_wrapper_ptr = wrapper_ptr;
			curs[i].max_retries = MAXRETRIES_SINGLETHREADED;
			solveSectionDiagonally(&curs[i]);
			percentSolvd = percentSolved(board);
			if(percentSolvd == 100)
				break;
		}
		tries++;
	}
	end = clock();
	printf("time taken: %.02lfms (diagonal solve, 1 thread) correct solution: %s\n", (double)(end - begin) / CLOCKS_PER_SEC*1000,
		isCorrect(board) ? "true" : "false");
	//reset board for next solve
	memcpy(board, reference, sizeof(int)*LIMIT*LIMIT);

	//9 threads, horizontal (one per row)
	begin = clock();
	for(i=0; i < 9; i++){
		pthread_attr_init(&attrs[i]);
		curs[i].row = 8 - i;
		curs[i].col = 0;
		curs[i].board_wrapper_ptr = wrapper_ptr;
		curs[i].max_retries = MAXRETRIES_MULTITHREADED;
		pthread_create(&tids[i], &attrs[i], solveSectionHorizontally, &curs[i]);
	}
	for(i=0; i < 9; i++)
		pthread_join(tids[i], NULL);
	end = clock();
	printf("time taken: %.02lfms (horizontal solve, 9 threads) correct solution: %s\n", (double)(end - begin) / CLOCKS_PER_SEC*1000,
		isCorrect(board) ? "true" : "false");
	//reset board for next solve
	memcpy(board, reference, sizeof(int)*LIMIT*LIMIT);

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
		curs[i].board_wrapper_ptr = wrapper_ptr;
		curs[i].max_retries = MAXRETRIES_MULTITHREADED;
		pthread_create(&tids[i], &attrs[i], solveSectionDiagonally, &curs[i]);
	}
	for(i=0; i < 17; i++)
		pthread_join(tids[i], NULL);
	end = clock();
	printf("time taken: %.02lfms (diagonal solve, 17 threads) correct solution: %s\n", (double)(end - begin) / CLOCKS_PER_SEC*1000,
		isCorrect(board) ? "true" : "false");
}

void *solveSectionDiagonally(void *thread_container) {
	thread_container_t *container = thread_container;
	int row = container->row;
	int col = container->col;
	int max_retries = container->max_retries;
	//macro for board, c version of 'int board[][] = board'
	int (*board)[LIMIT] = container->board_wrapper_ptr->board;

	int solved = 0; //num of non-empty cells in diagonal
	//caclulate how many cells this section contains (diagonal cells)
	int toSolve;
	if (row > col)
		toSolve = LIMIT - row;
	else
		toSolve = LIMIT - col;
	//keep going until all needed cells are solved or num of retries too high
	int retries = 0;
	int prevSolved = 0;

	while (solved < toSolve && retries < max_retries) {
		solved = 0;
		//progress diagonally
		int r = row, c = col;
		while (r < LIMIT && c < LIMIT) {
			sem_wait(&mutex);
			//solve cell
			if (board[r][c] == -1)
				solved = trySolve(board, r, c, solved);
			else
				solved++;
			sem_post(&mutex);
			r++;
			c++;
		}
		if (solved == prevSolved){
			sleep_ns(RETRY_WAIT_NS); //try to wait for another thread to update other cells
			retries++;
		}
		else
			retries = 0;
		prevSolved = solved;
	}
	return 0;
	pthread_exit(0);
}

void *solveSectionHorizontally(void *thread_container) {
	thread_container_t *container = thread_container;
	int row = container->row;
	int col = container->col;
	int max_retries = container->max_retries;
	//macro for board, c version of 'int board[][] = board'
	int (*board)[LIMIT] = container->board_wrapper_ptr->board;

	int solved = 0; //number of non-empty cells in row
	int toSolve = LIMIT; //number of cells per row
	//keep going until all needed cells are solved or num of retries too high
	int retries = 0;
	int prevSolved = 0;

	while (solved < toSolve && retries < max_retries) {
		solved = 0;
		//progress horizontally
		for (col=0; col < LIMIT; col++) {
			sem_wait(&mutex);
			//solve cell
			if (board[row][col] == -1)
				solved = trySolve(board, row, col, solved);
			else
				solved++;
			sem_post(&mutex);
		}
		if (solved == prevSolved){
			sleep_ns(RETRY_WAIT_NS); //try to wait for another thread to update other cells
			retries++;
		}
		else
			retries = 0;
		prevSolved = solved;
	}
	return 0;
	pthread_exit(0);
}

//subroutine to try to solve a cell
int trySolve(int board[][LIMIT], int row, int col, int solved) {
	bool used[10] = {0,0,0,0,0,0,0,0,0,0}; //tracks which numbers can't be candidates in [1]..[9]
	used[0] = true; //0 is never an option in sudoku
	int r, c;
	int retval = solved;
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
		retval++;
	} 
	else {
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
		//check if solution was found
		sum = 0;
		for (i=1; i < 10; i++){
			sum += !used[i];
			if(!used[i])
				last = i;
		}
		if (sum == 1){
			board[row][col] = last;
			retval++;
		}
	}
	return retval;
}

bool isCorrect(int board[][LIMIT]) {
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

int percentSolved(int board[][LIMIT]) {
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

void printBoard(int board[][LIMIT]) {
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

void printBoardComparison(int problem[][LIMIT], int solution[][LIMIT]) {
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

board_t readBoardFromFile(char *filepath){
	printf("reading from file: \'%s\'\n", filepath);
	FILE *inputfile = fopen(filepath, "r");
	board_t wrapper;
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
				wrapper.board[row][col] = -1;
			else
				wrapper.board[row][col] = (int)strtol(token, NULL, 0);
			token = strtok(NULL, " ");
			col++;
		}
		row++;
	}
	fclose(inputfile);
	return wrapper;
}

void writeBoardToFile(int board[][LIMIT], char *filepath) {
	printf("writing to file: \'%s\'\n", filepath);
	FILE *outputfile = fopen(filepath, "w");
	int row, col;
	for (row=0; row < LIMIT; row++) {
		for (col=0; col < LIMIT; col++) {
			fprintf(outputfile, "%d ", board[row][col]);
		}
		fprintf(outputfile, "\n");
	}
	fclose(outputfile);
}

void sleep_ns(int ns) //cross-platform sleep function (*nix)
{
	#if _POSIX_C_SOURCE >= 199309L
	    struct timespec ts;
	    ts.tv_sec = ns / 1000000;
	    ts.tv_nsec = ns;
	    nanosleep(&ts, NULL);
	#else
	    usleep(ns);
	#endif
}