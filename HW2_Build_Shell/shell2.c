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
    write(STDOUT_FILENO, "\n%s You typed Control-C!", 20);
}


int main()
{
    /* Share the prompt name since we want to update the parent after the child changing the name */
    prompt = (char*)mmap(NULL, sizeof(char)*1000, 0x1|0x2 , 0x01 | 0x20, -1, 0);
    strcpy(prompt, "hello:");

    char command[1024];
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

        /* parse command line */
        i = 0;
        // Breaks string str into a series of tokens
        token = strtok(command, " ");
        while (token != NULL)
        {
            argv[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        argv[i] = NULL;
        argcount = i;

        /* Is command empty */
        if (argv[0] == NULL)
            continue;

        /* Does command line end with & */
        if (!strcmp(argv[i - 1], "&"))
        {
            amper = 1;
            argv[i - 1] = NULL;
        }
        else
            amper = 0;

        /* Does command line end with > */
        if (!strcmp(argv[i - 2], ">"))
        {
            redirect = 1;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }
        /* Does command line end with 2> */
        else if (!strcmp(argv[i - 2], "2>"))
        {
            redirect = 2;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }
        else if (!strcmp(argv[i - 2], ">>"))
        {
            redirect = 3;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }
        else
        {
            redirect = 0;
        }

        // Change prompt name
        if (!strcmp(argv[0], "prompt") && !strcmp(argv[1], "=") && argv[2] != NULL)
        {
            strcpy(prompt, argv[2]);
        }

        // Print the last status command executed
        if (!strcmp(argv[0], "echo") && !strcmp(argv[1], "$?") && argcount == 2)
        {
            // int last_status = 1;
            printf("%d", WEXITSTATUS(status));
            printf("\n");
            continue;
        }

        // Change directory 
        if (!strcmp(argv[0], "cd"))
        {
            if (chdir(argv[1]))
            {
                printf("cd: %s: No such file or directory\n", argv[1]);
            }
        }
        
        //Exit from the program
        if (!strcmp(argv[0], "quit"))
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
            execvp(argv[0], argv);
        }

        /* parent continues here */
        if (amper == 0)
            retid = wait(&status);
    }
}
