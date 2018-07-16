#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "zenController.h"

static long long numArraysSorted = 0;
static int* arrayBeingSorted;
static pthread_t tid;
static int done = 0;
static int currentSize = 100;
static int nextSize = 100;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void swap(int* a, int* b);
static void bubbleSort(int* array, int length);
static int* generateNewArray(int length);
static void* sorterThread(void* arg);

// Begin/end the background thread which sorts random permutations.
void Sorter_startSorting(void) {
	pthread_create(&tid, NULL, *sorterThread, NULL);
}

void Sorter_stopSorting(void) {
	done = 1;
	pthread_join(tid, NULL);

	printf("Num arrays sorted: %llu\n", numArraysSorted);
}

// Set the size the next array to sort (don’t change current array)
void Sorter_setArraySize(int newSize) {
	nextSize = newSize;
}

// Get the size of the array currently being sorted.
int Sorter_getArrayLength(void) {
	return currentSize;
}

// Get a copy of the current (potentially partially sorted) array.
// Returns a newly allocated array and sets 'length' to be the 
// number of elements in the returned array (output-only parameter).
// The calling code must call free() on the returned pointer.
int* Sorter_getArrayData(int* length) {
	int* newArray;
	int i;

	pthread_mutex_lock(&mutex);

	int currArrayLength = Sorter_getArrayLength();

	newArray = malloc(currArrayLength * sizeof *newArray);
	for (i = 0; i < currArrayLength; i++) {
		newArray[i] = arrayBeingSorted[i];
	}

	*length = currArrayLength;

	pthread_mutex_unlock(&mutex);

	return newArray;
}

// Get the number of arrays which have finished being sorted.
long long Sorter_getNumberArraysSorted(void) {
	return numArraysSorted;
}

// swaps two ints by pointer
static void swap(int* a, int* b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}

static void bubbleSort(int* array, int length) {
	int i, j;

	for(i = 0; i < length-1; i++) {
		for (j = 0; j < length-i-1; j++) {
			if (array[j] > array[j+1]) {
				pthread_mutex_lock(&mutex);
				swap(&array[j], &array[j+1]);
				pthread_mutex_unlock(&mutex);			
			}
		}
	}
}

static int* generateNewArray(int length) {
	int* newArr = malloc(length * sizeof(int));
	int i, j;

	for (i = 0; i < length; i++) {
		newArr[i] = i;
	}

	for (i = 0; i < length; i++) {
		j = rand() % length;
		if (j != i) {
			swap(&newArr[i], &newArr[j]);	
		}
	}

	return newArr;
}

static void* sorterThread(void* arg) {
	int* temp;
	
	// for first free
	arrayBeingSorted = malloc(sizeof(int));

	while(!done) {
		nextSize = Zen_Pot_getArraySize();
		currentSize = nextSize;

		temp = generateNewArray(currentSize);

		pthread_mutex_lock(&mutex);
		free(arrayBeingSorted);
		arrayBeingSorted = temp;
		pthread_mutex_unlock(&mutex);

		bubbleSort(arrayBeingSorted, currentSize);
		numArraysSorted++;
	}
	free(arrayBeingSorted);

	return arrayBeingSorted;
}