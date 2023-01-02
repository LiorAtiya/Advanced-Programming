#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <sys/mman.h>

char lastCommand[1024];
char *prompt;

/* Command component linked list
   (generated after seperating the whole command with pipe "|") */
typedef struct Commands {
    char *command[10];
    struct Commands *next;
} Commands;

// https://stackoverflow.com/questions/779875/what-function-is-to-replace-a-substring-from-a-string-in-c
char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

// handler SIGINT
void ctrl_c_handler()
{
    printf("\nYou typed Control-C!\n");
    write(STDIN_FILENO, prompt, strlen(prompt)+1);
    write(STDIN_FILENO, " ", 1);
}

/* Close both sides of fd pipe */
void close_pipe(int fd[2]){
    close(fd[0]);
    close(fd[1]);
}

int main()
{
    /* Share the prompt name since we want to update the parent after the child changing the name */
    prompt = (char*)mmap(NULL, sizeof(char)*1000, 0x1|0x2 , 0x01 | 0x20, -1, 0);
    strcpy(prompt, "hello:");

    char command[1024];
    Commands *root;

    int pipes_num;
    int pipe_one[2];
    int pipe_two[2];
    char *token;
    char *outfile;
    int i, fd, amper, redirect, retid, status, argcount;
    char *argv[10];

    while (1)
    {

        // Handler with Ctrl + C
        signal(SIGINT, ctrl_c_handler);

        // Input a command from the keyboard
        printf("%s ", prompt);
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0';
    
        /* Update the lest command */
        strcpy(command, str_replace(command, "!!", lastCommand));
        if (strlen(command) != 0)
            strcpy(lastCommand, command);

        //Init linked list of commands (for pipes)
        root = (Commands *)malloc(sizeof(Commands));
        root->next = NULL;
        Commands *current = root;

        /* parse command line */
        i = 0;
        // Breaks string str into a series of tokens
        token = strtok(command, " ");
        while (token != NULL)
        {
            current->command[i] = token;
            token = strtok(NULL, " ");
            i++;
            if (token && !strcmp(token, "|")){
                token = strtok(NULL, " "); // skip empty space after "|"
                pipes_num++;
                current->command[i] = NULL;
                i = 0;
                Commands *next = (Commands *)malloc(sizeof(Commands));
                current->next = next;
                current = current->next;
                current->next = NULL;
                continue;
            }
        }
        current->command[i] = NULL;
        argcount = i;

        /* Is command empty */
        if (root->command[0] == NULL)
            continue;

        /* Does command line end with & */
        if (!strcmp(current->command[argcount -1], "&"))
        {
            amper = 1;
            root->command[argcount - 1] = NULL;
        }
        else
            amper = 0;

        while(current->next!=NULL) current = current->next;

        /* Does command line end with > */
        if (argcount > 1 && !strcmp(argv[argcount - 2], ">"))
        {
            redirect = 1;
            current->command[argcount - 2] = NULL;
            outfile = current->command[argcount - 1];
        }
        /* Does command line end with 2> */
        else if (argcount > 1 && !strcmp(argv[argcount - 2], "2>"))
        {
            redirect = 2;
            current->command[argcount - 2] = NULL;
            outfile = current->command[i - 1];
        }
        else if (argcount > 1 && !strcmp(argv[argcount - 2], ">>"))
        {
            redirect = 3;
            current->command[argcount - 2] = NULL;
            outfile = current->command[i - 1];
        }
        else
        {
            redirect = 0;
        }

        // Change prompt name
        if (!strcmp(current->command[0], "prompt") && !strcmp(current->command[1], "=") && current->command[2] != NULL)
        {
            strcpy(prompt, current->command[2]);
        }

        // Print the last status command executed
        if (!strcmp(current->command[0], "echo") && !strcmp(current->command[1], "$?") && argcount == 2)
        {
            // int last_status = 1;
            printf("%d", WEXITSTATUS(status));
            printf("\n");
            continue;
        }

        // Change directory 
        if (!strcmp(current->command[0], "cd"))
        {
            if (chdir(current->command[1]))
            {
                printf("cd: %s: No such file or directory\n", current->command[1]);
            }
        }
        
        //Exit from the program
        if (!strcmp(current->command[0], "quit"))
        {
            exit(0);
        }

        /* for commands not part of the shell command language */
        if (fork() == 0)
        {
            /* redirection of IO ? */
            // write to stdout
            if (redirect == 1)
            {
                fd = creat(outfile, 0660);
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
                /* stdout is now redirected */
            }
            // write to stderr
            else if (redirect == 2)
            {
                fd = creat(outfile, 0660);
                close(STDERR_FILENO);
                dup(fd);
                close(fd);
            }
            // add to exist file
            else if (redirect == 3)
            {
                // if exist file
                fd = open(outfile, O_WRONLY | O_APPEND);
                // if dosn't exist
                if (fd < 0)
                {
                    fd = creat(outfile, 0660);
                }
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
            }

            if (pipes_num > 0){
                current = root;
                /* Create pipe (0 = pipe output. 1 = pipe input) */
                pipe(pipe_one);
                /* for more then 1 pipe we need another pipe for chaining */
                if (pipes_num > 1)
                    pipe(pipe_two);
                /* For Iterating over all command components except the first & last components */
                int pipes_iterator = pipes_num - 1, pipe_switcher = 1, status = 1;
                /* Run first component (we need to get the input from the standart input) */
                pid_t pid = fork();
                if (pid == 0){
                    dup2(pipe_one[1], 1);
                    close(pipe_one[0]);
                    if (pipes_num > 1)
                        close_pipe(pipe_two);
                    execvp(current->command[0], current->command);
                    exit(0);
                } else {
                    waitpid(pid, &status, 0);
                    close(pipe_one[1]);
                    current = current->next;
                }
                /* Run [Iterate over] middle components (except first read name | echo heyand last)
                   while getting input from one pipe and output to other pipe */
                while (pipes_iterator > 0){
                    pid = fork();
                    if (pid == 0){
                        if (pipe_switcher % 2 == 1){
                            dup2(pipe_one[0], 0);
                            dup2(pipe_two[1], 1);
                        } else {
                            dup2(pipe_two[0], 0);
                            dup2(pipe_one[1], 1);
                        }
                        execvp(current->command[0], current->command);
                        exit(0);
                    } else {
                        waitpid(pid, &status, 0);
                        if (pipe_switcher % 2 == 1) {
                            close(pipe_two[1]);
                            close(pipe_one[0]);
                            pipe(pipe_one);
                        } else {
                            close(pipe_one[1]);
                            close(pipe_two[0]);
                            pipe(pipe_two);
                        }
                        current = current->next;
                        pipe_switcher++;
                        pipes_iterator--;
                    }
                }
                /* Run last component (we need to output to the standart output) */
                //pid = fork();
                //if (pid == 0) {
                    if (pipe_switcher % 2 == 0)
                        dup2(pipe_two[0], 0);
                    else
                        dup2(pipe_one[0], 0);

                    if (redirect == 1) { /* redirect stdout */
                        fd = creat(outfile, 0660);
                        dup2(fd, STDOUT_FILENO);
                    } else if (redirect == 3){ /* redirect stdout */
                        fd = open(outfile, O_CREAT | O_WRONLY | O_APPEND, 0660);
                        dup2(fd, STDOUT_FILENO);
                    } else if (redirect == 2) { /* redirect stderr */
                        fd = creat(outfile, 0660);
                        dup2(fd, STDERR_FILENO);
                    }

                    execvp(current->command[0], current->command);
                    exit(0);
                //} else {
                    //waitpid(pid, &status, 0);
                    close_pipe(pipe_one);
                    if (pipes_num > 1)
                        close_pipe(pipe_two);
                //}
            }else {
                execvp(current->command[0], current->command);
            }
        }

        /* parent continues here */
        if (amper == 0)
            retid = wait(&status);

        /* Free up all command components struct dynamic allocation */
        Commands *prev = root;
        current = root;
        for (int i = 0; i <= pipes_num; i++) {
            prev = current;
            current = current->next;
            free(prev);
        }
    }
}
