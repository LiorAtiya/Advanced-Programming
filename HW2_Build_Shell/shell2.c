#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>

int main()
{
    char *prompt = "hello";
    char command[1024];
    char *token;
    char *outfile;
    int i, fd, amper, redirect, retid, status, argcount;
    char *argv[10];

    FILE *fp;

    while (1)
    {
        // Input a command from the keyboard
        printf("%s: ", prompt);
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0';

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
            prompt = argv[2];
        }

        // Print the last status command executed
        if (!strcmp(argv[0], "echo") && !strcmp(argv[1], "$?") && argcount == 2)
        {
            int last_status = 1;
            printf("%d", WEXITSTATUS(status));
            printf("\n");
            continue;
        }

        // *********** NEED TO CHECK **************
        // Change directory 
        if (!strcmp(argv[0], "cd"))
        {
            char path[100];
            if (!chdir(argv[1]))
            {
                // Save the current directory in path
                if (getcwd(path, sizeof(path)) == NULL)
                {
                    perror("getcwd error! - path is NULL");
                }
            }
            else
            {
                perror("chdir error!");
            }
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
