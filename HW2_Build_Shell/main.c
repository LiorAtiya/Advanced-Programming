#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>

typedef struct node
{
    char name[10];
    char value[10];

} node;

// handler SIGINT
void ctrl_c_handler()
{
    write(STDOUT_FILENO, "\nYou typed Control-C!", 20);
}

int if_exists_in_array(char *target, node n[], int size_of_array)
{

    for (int index = 0; index < size_of_array; index++)
    {
        if (!strcmp(n[index].name, target))
        {
            return index;
        }
    }
    return -1;
}

int main()
{
    char command[1024];
    char *token;
    int i;
    char *outfile;
    int fd, amper, redirect, piping, retid, status, argc1;
    int fildes[2];
    char *argv1[10], *argv2[10];
    char *prompt_name = "hello";
    int last_command_size = 0;
    char *last_command[10];
    node variable[100];
    int index_of_variable = 0;

    while (1)
    {
        // Handler with Ctrl + C
        signal(SIGINT, ctrl_c_handler);

        // Input a command from the keyboard
        printf("%s: ", prompt_name);
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0';
        piping = 0;

        /* parse command line */
        i = 0;
        // Breaks string str into a series of tokens
        token = strtok(command, " ");
        while (token != NULL)
        {
            argv1[i] = token;
            token = strtok(NULL, " ");
            i++;
            // if char == '|'
            if (token && !strcmp(token, "|"))
            {
                piping = 1;
                break;
            }
        }
        argv1[i] = NULL;
        argc1 = i;

        /* Is command empty */
        if (argv1[0] == NULL)
            continue;

        // Command contain pipe
        if (piping)
        {
            i = 0;
            while (token != NULL)
            {
                token = strtok(NULL, " ");
                argv2[i] = token;
                i++;
            }
            argv2[i] = NULL;
        }

        // Command line end with & 
        if (!strcmp(argv1[argc1 - 1], "&"))
        {
            amper = 1;
            argv1[argc1 - 1] = NULL;
        }
        else
        {
            amper = 0;
        }

        if (argc1 > 1 && !strcmp(argv1[argc1 - 2], ">"))
        {
            redirect = 1;
            argv1[argc1 - 2] = NULL;
            outfile = argv1[argc1 - 1];
        }
        else
        {
            redirect = 0;
        }
        if (argv1[1] != NULL && !strcmp(argv1[1], "$?") && argc1 == 2)
        {
            int last_status = 1;
            printf("%d", WEXITSTATUS(status));
            printf("\n");
            continue;
        }
        // redirect to stderr
        if (argv1[i - 2] != NULL && !strcmp(argv1[i - 2], "2>"))
        {
            redirect = 2;
            argv1[i - 2] = NULL;
            outfile = argv1[i - 1];
        }

        if (!strcmp(argv1[0], "quit"))
        {
            exit(0);
        }

        // 6. copy last command to execute last command
        if (!strcmp(argv1[0], "!!"))
        {
            for (int v = 0; v < last_command_size; v++)
            {
                argv1[v] = (char *)malloc(strlen(last_command[v]) + 1);
                strcpy(argv1[v], last_command[v]);
            }
        }
        // copy my current command
        else if (strcmp(argv1[0], "!!"))
        {
            for (int v = 0; v < i && argv1[v] != NULL; v++)
            {
                last_command[v] = (char *)malloc(strlen(argv1[v]) + 1);
                strcpy(last_command[v], argv1[v]);
                last_command_size = v + 1;
            }
        }

        /* for commands not part of the shell command language */
        if (fork() == 0)
        {
            /* redirection of IO ? */
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
                fd = creat(outfile, 0660); // equal to open(outfile,O_WRONLY|O_CREAT|O_TRUNC,mode)
                close(STDERR_FILENO);
                dup(fd);
                close(fd);
            }

            // change prompt name
            if (!strcmp(argv1[0], "prompt") && !strcmp(argv1[1], "=") && argv1[2] != NULL)
            {
                prompt_name = argv1[2];
            }

            // print to terminal
            if (!strcmp(argv1[0], "echo"))
            {
                int k = 1;
                while (argv1[k] != NULL)
                {
                    int exists_val = if_exists_in_array(argv1[k], variable, 100);
                    if (exists_val != -1)
                    {
                        printf("%s ", variable[exists_val].value);
                    }
                    else
                    {
                        printf("%s ", argv1[k]);
                    }
                    k++;
                }
                printf("\n");
                continue;
            }

            // change directory
            if (!strcmp(argv1[0], "cd"))
            {
                char path[100];

                if (!chdir(argv1[1]))
                {
                    // Save the current directory in path
                    if (getcwd(path, sizeof(path)) == NULL)
                    {
                        perror("getcwd error! - path is NULL");
                    }
                }
                else
                {
                    perror("chdir error!"); // not succeed to change the path
                }
            }
            
            // save and use variable
            if (argv1[0][0] == '$' && !strcmp(argv1[1], "=") && argv1[2] != NULL && argv1[3] == NULL)
            {
                int answer_exists = if_exists_in_array(argv1[0], variable, 100);
                if (answer_exists != -1)
                {
                    strcpy(variable[answer_exists].value, argv1[2]);
                }
                else
                {
                    node *n0 = NULL;
                    n0 = (struct node *)malloc(sizeof(struct node));
                    strcpy(n0->name, argv1[0]);
                    strcpy(n0->value, argv1[2]);
                    variable[index_of_variable] = *n0;
                    index_of_variable++;
                }
            }
            // read commad: save user value
            if (!strcmp(argv1[0], "read") && argv1[1] != NULL && argv1[2] == NULL)
            {
                char user_value[20];
                scanf("%[^\n]%*c", user_value); // get value from user
                char sign[50] = "$";
                strcat(sign, argv1[1]);
                int answer_exists = if_exists_in_array(sign, variable, 100);

                if (answer_exists != -1)
                {
                    strcpy(variable[answer_exists].value, user_value);
                }
                else
                {
                    node *n0 = NULL;
                    n0 = (struct node *)malloc(sizeof(struct node));
                    strcpy(n0->name, sign);
                    strcpy(n0->value, user_value);
                    variable[index_of_variable] = *n0;
                    index_of_variable++;
                }
            }

            if (piping)
            {
                pipe(fildes);
                if (fork() == 0)
                {
                    /* first component of command line */
                    close(STDOUT_FILENO);
                    dup(fildes[1]);
                    close(fildes[1]);
                    close(fildes[0]);
                    /* stdout now goes to pipe */
                    /* child process does command */
                    execvp(argv1[0], argv1);
                }
                /* 2nd command component of command line */
                close(STDIN_FILENO);
                dup(fildes[0]);
                close(fildes[0]);
                close(fildes[1]);
                /* standard input now comes from pipe */
                execvp(argv2[0], argv2);
            }
            else
                execvp(argv1[0], argv1);
        }
        /* parent continues over here... */
        /* waits for child to exit if required */
        if (amper == 0)
            retid = wait(&status);
    }
}