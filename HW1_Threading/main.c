#include "codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUFFERSIZE 1024
#define THREAD_NUM 4

// global variables
int key;
char *flag;
char arr[256][10];

typedef struct Task
{
	int index;
	char buffer[6];
} Task;

Task taskQueue[256];
int taskCount = 0;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

void executeTask(Task *task)
{
	// printf("thread: %s\n",task->buffer);

	// Check type of flag
	if (strcmp(flag, "-e") == 0)
	{
		encrypt(task->buffer, key);
		memcpy(&arr[task->index], task->buffer, sizeof(task->buffer));
		// printf("enc: %s\n", task->buffer);
	}
	else if (strcmp(flag, "-d") == 0)
	{
		decrypt(task->buffer, key);
		memcpy(&arr[task->index], task->buffer, sizeof(task->buffer));
		// printf("%s", task->buffer);
	}
}

void submitTask(Task task)
{
	pthread_mutex_lock(&mutexQueue);
	taskQueue[taskCount] = task;
	taskCount++;
	pthread_mutex_unlock(&mutexQueue);
	pthread_cond_signal(&condQueue);
}

void *startThread(void *args)
{
	while (taskCount)
	{
		Task task;

		pthread_mutex_lock(&mutexQueue);
		while (taskCount == 0)
		{
			pthread_cond_wait(&condQueue, &mutexQueue);
		}

		task = taskQueue[0];
		int i;
		for (i = 0; i < taskCount - 1; i++)
		{
			taskQueue[i] = taskQueue[i + 1];
		}
		taskCount--;

		pthread_mutex_unlock(&mutexQueue);

		executeTask(&task);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	// Check valid input
	if (argc != 3)
	{
		printf("Too few arguments \n");
		printf("USAGE: ./coder key flag < file.txt > out.txt\n");
		exit(0);
	}

	// Get data from input args
	key = atoi(argv[1]);
	flag = argv[2];

	if (strcmp(flag, "-e") != 0 && strcmp(flag, "-d") != 0)
	{
		printf("Error flag: Enter -e/-d\n");
		exit(0);
	}

	pthread_t threads[THREAD_NUM];
	pthread_mutex_init(&mutexQueue, NULL);
	pthread_cond_init(&condQueue, NULL);

	// Read file from the input
	char buffer[5]; // Buffer to store data
	int index = 0;
	while (fgets(buffer, sizeof(buffer), stdin) != NULL)
	{
		Task newTask;
		newTask.index = index;
		memcpy(&newTask.buffer, buffer, sizeof(buffer));
		// printf("index: %d | buffer: %s\n", newTask.index, newTask.buffer);
		submitTask(newTask);
		index++;
	}

	int i;

	// Create threads
	for (i = 0; i < THREAD_NUM; i++)
	{
		if (pthread_create(&threads[i], NULL, &startThread, NULL) != 0)
		{
			perror("Failed to create the thread");
		}
	}

	// Join threads
	for (i = 0; i < THREAD_NUM; i++)
	{
		if (pthread_join(threads[i], NULL) != 0)
		{
			perror("Failed to join the thread");
		}
	}

	pthread_cond_destroy(&condQueue);
	pthread_mutex_destroy(&mutexQueue);

	for (int i = 0; i < 10; i++)
	{
		printf("arr after encrypt/decrypt: %s\n", arr[i]);
	}

	return 0;
}
