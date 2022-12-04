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
	char buffer[10];
} Task;

Task taskQueue[256];
int taskCount = 0;
//for index of per task
int count = 0;
int executed = 0;
int notFinished = 1;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

void executeTask(Task *task)
{
	// Check type of flag
	if (strcmp(flag, "-e") == 0)
	{
		encrypt(task->buffer, key);
	}
	else if (strcmp(flag, "-d") == 0)
	{
		decrypt(task->buffer, key);
	}

	//Checks the next in the executed tasks 
	while (1)
	{
		// printf("exe: %d | index: %d\n",executed,task->index);
		if(executed == task->index){
			printf("%s", task->buffer);
			executed++;
			break;
		}else if(executed > task->index){
			break;
		}
	}
}

void *startThread(void *args)
{
	// there are still tasks or not finished reading the file
	while (taskCount > 0 || notFinished)
	{
		pthread_mutex_lock(&mutexQueue);
		char buffer[5]; // Buffer to store data
		// Read file
		if (fgets(buffer, sizeof(buffer), stdin) != NULL)
		{
			Task newTask;
			newTask.index = count;
			memcpy(&newTask.buffer, buffer, sizeof(buffer));
			count++;

			// Add to queue of tasks
			taskQueue[taskCount] = newTask;
			taskCount++;
		}
		else
		{
			notFinished = 0;
		}

		Task task;
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

	// Checks valid flag
	if (strcmp(flag, "-e") != 0 && strcmp(flag, "-d") != 0)
	{
		printf("Error flag: Enter -e/-d\n");
		exit(0);
	}

	// Take number of cores of computer from the system
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

	// for (int i = 0; i < 10; i++)
	// {
	// 	printf("%s", arr[i]);
	// }

	return 0;
}
