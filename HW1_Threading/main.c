#include "codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUFFERSIZE 1024
#define THREAD_NUM 4

//global variables
int key;
char* flag;

void* encryptAndDecrypt(){
	//Get data from file
	char buffer[BUFFERSIZE];
	fgets(buffer, BUFFERSIZE,stdin);
	
	//Check type of flag
	if(strcmp(flag,"-e") == 0){
		encrypt(buffer,key);
		printf("%s",buffer);
	}else if(strcmp(flag,"-d") == 0){
		decrypt(buffer,key);
		printf("%s",buffer);
	}else{
		printf("Error flag: Enter -e/-d\n");
		exit(0);
	}
}

int main(int argc, char *argv[])
{	

	//Check valid input
	if(argc != 3){
		printf("Error args: Need just 2 arguments - key, flag\n");
		exit(0);
	}

	//Get data from input args
	key = atoi(argv[1]);
	flag = argv[2];

	pthread_t threads[THREAD_NUM];
	int i;
	//Create threads
	for(i = 0 ; i < THREAD_NUM ; i++){
		if(pthread_create(threads + i,NULL,&encryptAndDecrypt, NULL) != 0){
			perror("Failed to create the thread");
		}
	}

	//Join threads
	for(i = 0 ; i < THREAD_NUM ; i++){
		if(pthread_join(threads[i], NULL) != 0){
			perror("Failed to join the thread");
		}
	}

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
