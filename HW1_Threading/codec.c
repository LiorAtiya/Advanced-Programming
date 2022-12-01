#include "codec.h"
#include <string.h>
#include <sys/time.h>

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}

void encrypt(char *s,int key)
{
    int i, l = strlen(s);
    if (l > 1024) l = 1024;

    long long cur_time = current_timestamp();
    long long expected_time = cur_time + 10*l;
    while (cur_time < expected_time){
	cur_time = current_timestamp();
    }	

    for(i = 0; i < l; i++)
        s[i] -= key;
}
void decrypt(char *s,int key)
{
    int i, l = strlen(s);
    if (l > 1024) l = 1024;

    long long cur_time = current_timestamp();
    long long expected_time = cur_time + 10*l;
    while (cur_time < expected_time){
	cur_time = current_timestamp();
    }
    for(i = 0; i < l; i++)
        s[i] += key;
}
