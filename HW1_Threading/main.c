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

// void *encryptAndDecrypt()
// {
// 	// Get data from file
// 	char buffer[BUFFERSIZE];
// 	fgets(buffer, BUFFERSIZE, stdin);

// 	// //Check type of flag
// 	// if(strcmp(flag,"-e") == 0){
// 	// 	encrypt(buffer,key);
// 	// 	printf("%s",buffer);
// 	// }else if(strcmp(flag,"-d") == 0){
// 	// 	decrypt(buffer,key);
// 	// 	printf("%s",buffer);
// 	// }else{
// 	// 	printf("Error flag: Enter -e/-d\n");
// 	// 	exit(0);
// 	// }
// }

typedef struct Task
{
	char buffer[5];
} Task;

Task taskQueue[256];
int taskCount = 0;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

void executeTask(Task *task)
{
	// Check type of flag
	if (strcmp(flag, "-e") == 0)
	{
		encrypt(task->buffer, key);
		printf("%s", task->buffer);
	}
	else if (strcmp(flag, "-d") == 0)
	{
		decrypt(task->buffer, key);
		printf("%s", task->buffer);
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
	while (1)
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
}

int main(int argc, char *argv[])
{
	// Check valid input
	if (argc != 3)
	{
		printf("Error args: Need just 2 arguments - key, flag\n");
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

	char buffer[5]; // Buffer to store data

	while (fgets(buffer, sizeof(buffer), stdin) != NULL)
	{
		Task newTask;
		memcpy(&newTask, buffer, sizeof(buffer));
		submitTask(newTask);
	}

	pthread_t threads[THREAD_NUM];
	pthread_mutex_init(&mutexQueue, NULL);
	pthread_cond_init(&condQueue, NULL);
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

	//============================================================
	// //Check type of flag
	// if(strcmp(flag,"-e") == 0){
	// 	encrypt(buffer,key);
	// 	printf("%s",buffer);
	// }else if(strcmp(flag,"-d") == 0){
	// 	decrypt(buffer,key);
	// 	printf("%s",buffer);
	// }else{
	// 	printf("Error flag: Enter -e/-d\n");
	// 	exit(0);
	// }

	// char data[] = "my text to encrypt";
	// encrypt(buffer,key);
	// printf("encripted data: %s\n",buffer);

	// decrypt(data,key);
	// printf("decripted data: %s\n",data);

	return 0;
}
