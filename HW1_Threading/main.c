#include "codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFERSIZE 1024

// global variables
int key;
char *flag;
char arr[256][1024];

typedef struct Task
{
	int index;
	char bufferIN[10];
	char bufferOUT[1025];
} Task;

Task taskQueue[256];
int taskCount = 0;
int count = 0;
int first = 1;
int isDone = 1;
pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

void executeTask(Task *task)
{
	// Check type of flag
	if (strcmp(flag, "-e") == 0)
	{
		encrypt(task->bufferIN, key);
		memcpy(&arr[task->index], task->bufferIN, sizeof(task->bufferIN));
	}
	else if (strcmp(flag, "-d") == 0)
	{
		decrypt(task->bufferIN, key);
		memcpy(&arr[task->index], task->bufferIN, sizeof(task->bufferIN));
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
	while (taskCount > 0 || isDone)
	{
		pthread_mutex_lock(&mutexQueue);
		char buffer[5]; // Buffer to store data
		if (fgets(buffer, sizeof(buffer), stdin) != NULL)
		{
			Task newTask;
			newTask.index = count;
			memcpy(&newTask.bufferIN, buffer, sizeof(buffer));
			count++;

			taskQueue[taskCount] = newTask;
			taskCount++;

		}else {
			isDone = 0;
		}

		Task task;

		// while (taskCount == 1)
		// {
		// 	pthread_cond_wait(&condQueue, &mutexQueue);
		// }

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

	int num_of_cores = sysconf(_SC_NPROCESSORS_CONF);

	pthread_t threads[num_of_cores];
	pthread_mutex_init(&mutexQueue, NULL);
	pthread_cond_init(&condQueue, NULL);

	int i;
	// Create threads
	for (i = 0; i < num_of_cores; i++)
	{
		if (pthread_create(&threads[i], NULL, &startThread, NULL) != 0)
		{
			perror("Failed to create the thread");
		}
	}

	// Join threads
	for (i = 0; i < num_of_cores; i++)
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
		printf("%s", arr[i]);
	}

	return 0;
}
