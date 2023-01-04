#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>
#include <sys/mman.h>

int main_pid;
char *prompt;
char command[1024];
char lastCommand[1024];
int es = 0;
int errInCd = 0;
char *infile;
char *outfile;
int fd = -2;

//For if else statement
enum states { NEUTRAL, WANT_THEN, WANT_ELSE, ELSE_BLOCK,THEN_BLOCK };
enum results { SUCCESS, FAIL };

int if_state = NEUTRAL;
int if_result = SUCCESS;
int last_stat = 0;
int rv = -1;

// Key_value linked list = store named variables
typedef struct key_value {
   char* key;
   char* value;
   struct key_value *next;
} key_value;

key_value *key_value_root;

// Commands linked list (generated after seperating the whole command with pipe "|")
typedef struct Commands {
    char *command[10];
    struct Commands *next;
} Commands;

Commands *root;

// last_commands linked list for store all of recent commands
typedef struct last_commands {
    char command[1024];
    struct last_commands *next;
    struct last_commands *prev;
} last_commands;

last_commands *last_commands_root; 

void key_value_add(char* key, char* value) {
    key_value *iter = key_value_root;
    while (iter->next != NULL) {
        iter = iter->next;
        if (strcmp(iter->key,key) == 0) {
            char* new_val = malloc(strlen(value) + 1); 
            strcpy(new_val, value);
            iter->value = new_val;
            return;
        }
    }
    key_value *next = (key_value*) malloc(sizeof(key_value));
    char* new_key = malloc(strlen(key) + 1); 
    strcpy(new_key, key);
    next->key = new_key;
    char* new_val = malloc(strlen(value) + 1); 
    strcpy(new_val, value);
    next->value = new_val;
    next->next = NULL;
    iter->next = next;
    return;
}

char* value_get(char* key) {
    key_value *iter = key_value_root;
    while (iter->next != NULL) {
        iter = iter->next;
        if (strcmp(iter->key,key) == 0) {
            return iter->value;
        }
    }
    return NULL;
}

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

// Handle with Ctrl+C input
void ctrl_c_handler(int dummy) {
    strcpy(lastCommand, "^C");
    if (dummy == SIGTSTP) {
        exit(0);
    }
    if (getpid() == main_pid) {
        printf("\nYou typed Control-C!\n");
        char message[2] = " ";
        write(STDIN_FILENO, prompt, strlen(prompt)+1);
        write(STDIN_FILENO, message, strlen(message)+1);
    }
}

// Handle with arrow up & down
void handle_arrows(char *token){

    if(token != NULL && !strcmp(token,"\033[A")){ // arrow up
    
        if(last_commands_root->prev != NULL){ 
            strcpy(command, last_commands_root->prev->command);
            last_commands_root = last_commands_root->prev;
            token = strtok(command, " ");
        }else{
            strcpy(command, last_commands_root->command);
        }
        
    } else if(token != NULL && !strcmp(token,"\033[B")){ // arrow down

        if(last_commands_root->next != NULL){
            strcpy(command, last_commands_root->next->command);
            last_commands_root = last_commands_root->next;
            token = strtok(command, " ");
        }else{
            strcpy(command, last_commands_root->command);
        }

    } else if (token != NULL && strlen(command) != 0){
        
        while(last_commands_root->next != NULL){
            last_commands_root = last_commands_root->next;
        }

        // Insert command to current last_commands
        strcpy(last_commands_root->command, lastCommand);

        last_commands *next = (last_commands*) malloc(sizeof(last_commands));
        next->prev = last_commands_root;
        last_commands_root->next = next;
        last_commands_root = last_commands_root->next;
        last_commands_root->next = NULL;
    }
}

// Check if typed 'quit'
void check_quit(char *token)
{
    if (strcmp(token, "quit") == 0){
        exit(0);
    }
    if (strcmp(token, "^C") == 0){
        printf("You typed Control-C!\n");
    }
}

// Check if typed 'echo $?' => Print the status of last command executed
void handle_exit_status(Commands *list, int status) {
    while (list != NULL){
        for (int i = 0; i < 9; i++){
            if (list->command[i+1] == NULL){break;}
            if (strcmp(list->command[i], "echo") == 0 && strcmp(list->command[i+1], "$?") == 0) {
                if (!errInCd && WIFEXITED(status)) {
                    es = WEXITSTATUS(status);
                }
                else if (errInCd) {
                    es = 1;
                    errInCd = 0;
                }
                sprintf(list->command[i+1], "%d", es);
            }
        }
        list = list->next;
    }
}

// Check if typed 'cd'
int handle_cd_command(char *token1, char *token2) {
    if (token2 != NULL && !strcmp(token1, "cd")) {
        if (chdir(token2)){
            printf("cd: %s: No such file or directory\n", token2);
            errInCd = 1;
        }
        return 1;
    }
    return 0;
}

// Insert new variables to sturct key_value / Replace key with value for output
void handle_variables(Commands *root,int pipes_num) {
    Commands *iterator = root;
    int num = pipes_num + 1;
    while (num > 0) {
        if (iterator->command[0] == NULL){
            num--;
            iterator = iterator->next;
            continue;
        }
        //Check template of: $key = value
        if (iterator->command[0][0] == '$' && strcmp(iterator->command[1],"=") == 0) {
            key_value_add(iterator->command[0]+1, iterator->command[2]);
        }
        num--;
        iterator = iterator->next;
    }
    iterator = root;
    num = pipes_num + 1;
    while (num > 0) {
        if (iterator->command[0] == NULL) {
            num--;
            iterator = iterator->next;
            continue;
        }
        //Replace key with value
        if (!(iterator->command[0][0] == '$' && strcmp(iterator->command[1],"=") == 0)) {
            for (int i = 0; i < 10; i++) {
                if (iterator->command[i] != NULL && iterator->command[i][0] == '$') {
                    char *val = value_get(iterator->command[i]+1);
                    if (val!=NULL) iterator->command[i] = val;
                    else iterator->command[i] = ""; // if the key doesnt found
                }}}
        num--;
        iterator = iterator->next;
    }
}

// Add new variable with 'read'
void handle_read_command(){
    if (root->command[0] != NULL && root->command[1] != NULL && strcmp(root->command[0],"read") == 0) {
        char *key = root->command[1];
        char value[20];
        fgets(value, 20, stdin);
        value[strlen(value)-1] = '\0'; // remove new line ('\n')
        char *val = value;
        key_value_add(key, val);
    }
}

// Check if typed '&' in the end
int handle_ampersand_command(Commands *current, int args_count) {
    // Does command line end with &
    if (!strcmp(current->command[args_count - 1], "&")){
        root->command[args_count - 1] = NULL;
        return 1;
    } else
        return 0;
}

// Checks if typed '<' / '>' / '>>' / '2>'
int handle_redirection(int args_count, Commands *current){
    if (args_count > 1 && !strcmp(current->command[args_count - 2], ">" )){
        current->command[args_count - 2] = NULL;
        outfile = current->command[args_count - 1];
        return 1;
    }
    else if (args_count > 1 && !strcmp(current->command[args_count - 2], "2>" )){
        current->command[args_count - 2] = NULL;
        outfile = current->command[args_count - 1];
        return 2;
    }
    else if (args_count > 1 && !strcmp(current->command[args_count - 2], ">>")){
        current->command[args_count - 2] = NULL;
        outfile = current->command[args_count - 1];
        return 3;
    }
    else if (args_count > 1 && !strcmp(current->command[args_count - 2], "<")){
        current->command[args_count - 2] = NULL;
        infile = current->command[args_count - 1];
        return 4;
    }
    else
        return 0;
}

// Check if typed 'prompt = newPrompt'
void change_prompt(char *token1, char *token2, char *token3) {
    if (token1 != NULL && token2 != NULL && token3 != NULL && strcmp(token1, "prompt") == 0 
            && strcmp(token2, "=") == 0) {
        strcpy(prompt, token3);
    }
}

// Execute redirect by the flag
void execute_redirection(int redirect, int pipes_num) {
    if (redirect == 1)
        { /* redirect stdout */
            fd = creat(outfile, 0660);
            if (pipes_num == 0) {
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
            }
        }
        if (redirect == 2)
        { /* redirect stderr */
            fd = creat(outfile, 0660);
            if (pipes_num == 0) {
                close(STDERR_FILENO);
                dup(fd);
                close(fd);
            }
        }
        else if (redirect == 3)
        { /* redirect stdout */
            fd = open(outfile, O_CREAT | O_WRONLY | O_APPEND, 0660);
            if (pipes_num == 0) {
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
            }
        }
        else if (redirect == 4)
        { /* redirect stderr */
            fd = open(infile, O_RDONLY | O_CREAT);
            if (pipes_num == 0) {
                close(STDIN_FILENO);
                dup(fd);
                close(fd);
            }
        }
}

// Error massage for if/else statment
int syn_err(char *msg){
    fprintf(stderr,"syntax error: %s\n", msg);
    return -1;
}

// Handle with all conditions of if / else statment
int handle_if_else_statment(Commands *current, int pipes_num) {
    //If state is fail (else block) => ignore from commands
        if(if_state == THEN_BLOCK){
            if (if_result == FAIL && strcmp(root->command[0],"else")){

                // Free up all commands struct dynamic allocation
                Commands *prev = root;
                current = root;
                for (int i = 0; i <= pipes_num; i++) {
                    prev = current;
                    current = current->next;
                    free(prev);
                }

                return -1;
            }
        }

        //If state is success (then block) => ignore from commands
        if(if_state == ELSE_BLOCK){
            if (if_result == SUCCESS && strcmp(root->command[0],"fi")){

                // Free up all commands struct dynamic allocation
                Commands *prev = root;
                current = root;
                for (int i = 0; i <= pipes_num; i++) {
                    prev = current;
                    current = current->next;
                    free(prev);
                }

                return -1;
            }
        }

        //After the 'if' need to come 'then'
        if((if_state == WANT_THEN || if_state == WANT_ELSE) && strcmp(root->command[0],"then")){
            rv = syn_err("then expected");
            return -1;
        }

        if(strcmp(root->command[0],"if") == 0){
            if(if_state != NEUTRAL){
                rv = syn_err("if expected");
            }else {
                //Execute command after the 'if'
                char without_if[1024];
                strncpy(without_if, lastCommand + 3, sizeof(lastCommand) - 3);

                //Check if the condition is true
                last_stat = system(without_if);

                if_result = (last_stat == 0? SUCCESS : FAIL);
                if(if_result == SUCCESS){
                    if_state = WANT_THEN;
                }else{
                    if_state = WANT_ELSE;
                }
            }
        }else if (strcmp(root->command[0],"then") == 0){
            if (if_state == THEN_BLOCK) {
                rv = syn_err("else expected");
            }
            else if (if_state == NEUTRAL)
            {
                rv = syn_err("if expected");
            }
            else if (if_state == ELSE_BLOCK) {
                rv = syn_err("fi expected");
            }
            else {
                if_state = THEN_BLOCK;
            }
        }
        else if(strcmp(root->command[0],"else") == 0){
            if(if_state == NEUTRAL){
                rv = syn_err("if expected");
            }else if (if_state == WANT_THEN){
                rv = syn_err("then expected");
            }else {
                if_state = ELSE_BLOCK;
            }
        }
        else if (strcmp(root->command[0],"fi") == 0) {
            if (if_state == WANT_THEN) {
                rv = syn_err("then expected");
            }
            else if (if_state == NEUTRAL){
                rv = syn_err("if expected");
            }
            else if(if_state == THEN_BLOCK) {
                rv = syn_err("else expected");
            }
            else {
                if_state = NEUTRAL;
                if_result = SUCCESS;
            }
        }
}

int main(){
    //Share the prompt name since we want to update the parent after the child changing the name
    prompt = (char*)mmap(NULL, sizeof(char)*1000, 0x1|0x2 , 0x01 | 0x20, -1, 0);
    strcpy(prompt, "hello:");
    
    main_pid = getpid();
    char *token;
    int i;
    int amper, redirect, status, args_count;

    //For pipes
    int pipe_one[2];
    int pipe_two[2];
    int pipes_num;

    //For store variables
    key_value_root = (key_value*) malloc(sizeof(key_value));
    key_value_root->next = NULL;

    //For store last commands
    last_commands_root = (last_commands*) malloc(sizeof(last_commands));
    last_commands_root->next = NULL;
    last_commands_root->prev = NULL;

    while (1) {
        
        // 8.Handler with Ctrl + C
        signal(SIGINT, ctrl_c_handler);

        printf("%s ", prompt);

        //Input new command / commands (with pipes)
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0';

        // Update lest command
        strcpy(command, str_replace(command, "!!", lastCommand));
        if (strlen(command) != 0){
            strcpy(lastCommand, command);
        }

        i = 0;
        pipes_num = 0;

        token = strtok(command, " ");

        handle_arrows(token);

        root = (Commands *)malloc(sizeof(Commands));
        root->next = NULL;
        Commands *current = root;
        while (token != NULL){

            // Exit from the program
            check_quit(token);

            current->command[i] = token;
            token = strtok(NULL, " ");
            i++;

            // There is pipes => allocate new place for new command
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
        args_count = i;
        
        // Checks of command empty
        if (root->command[0] == NULL)
            continue;

        if(handle_if_else_statment(current,pipes_num) == -1){
            continue;
        }

        handle_exit_status(root, status);

        handle_cd_command(root->command[0], root->command[1]);

        handle_variables(root, pipes_num);

        handle_read_command();
        
        amper = handle_ampersand_command(current, args_count);

        // while(current->next != NULL) current = current->next;

        redirect = handle_redirection(args_count, current);

        change_prompt(current->command[0],current->command[1],current->command[2]);

        // for commands not part of the shell command language
        if (fork() == 0) {

            execute_redirection(redirect, pipes_num);

            // Handle pipes 
            if (pipes_num > 0){
                current = root;
                // Create pipe (0 = pipe output. 1 = pipe input) 
                pipe(pipe_one);
                // for more then 1 pipe we need another pipe for chaining 
                if (pipes_num > 1)
                    pipe(pipe_two);
                // For Iterating over all commands except the first & last components 
                int pipes_iterator = pipes_num - 1, pipe_switcher = 1, status = 1;
                // Run first component (we need to get the input from the standart input)
                pid_t pid = fork();
                if (pid == 0){
                    dup2(pipe_one[1], 1);
                    close(pipe_one[0]);
                    if (pipes_num > 1)
                        // Close both sides of fd pipe
                        close(pipe_two[0]);
                        close(pipe_two[1]);
                    execvp(current ->command[0], current ->command);
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
                            // Close both sides of fd pipe
                            close(pipe_two[1]);
                            close(pipe_one[0]);
                            pipe(pipe_one);
                        } else {
                            // Close both sides of fd pipe
                            close(pipe_one[1]);
                            close(pipe_two[0]);
                            pipe(pipe_two);
                        }
                        current = current->next;
                        pipe_switcher++;
                        pipes_iterator--;
                    }
                }
                // Run last component (we need to output to the standart output)
                    if (pipe_switcher % 2 == 0)
                        dup2(pipe_two[0], 0);
                    else
                        dup2(pipe_one[0], 0);

                    // /* 1. ---------- Handle Redirection (In pipes) ---------- */
                    // if (redirect == 1) { /* redirect stdout */
                    //     fd = creat(outfile, 0660);
                    //     dup2(fd, STDOUT_FILENO);
                    // } else if (redirect == 3){ /* redirect stdout */
                    //     fd = open(outfile, O_CREAT | O_WRONLY | O_APPEND, 0660);
                    //     dup2(fd, STDOUT_FILENO);
                    // } else if (redirect == 2) { /* redirect stderr */
                    //     fd = creat(outfile, 0660);
                    //     dup2(fd, STDERR_FILENO);
                    // } else if (redirect == 4){ /* redirect stderr */
                    //     fd = open(infile, O_RDONLY | O_CREAT); 
                    //     dup2(fd, STDIN_FILENO);
                    // }
                    
                    // /* ---------- End of handle redirection (In pipes) ---------- */

                    execvp(current->command[0], current->command);
                    exit(0);
                    //Close both sides of fd pipe
                    close(pipe_one[0]);
                    close(pipe_one[1]);
                    // close_pipe(pipe_one);

                    if (pipes_num > 1)
                        //Close both sides of fd pipe
                        close(pipe_one[0]);
                        close(pipe_one[1]);
                        // close_pipe(pipe_two);
            } else {
                execvp(root->command[0], root->command);
            }
            // In case of unknown command let the child exit
            exit(0);
        }

        /* Parent continues over here
           (waits for child to exit if required) */
        if (amper == 0)
            wait(&status); // retid = wait(&status);


        // Free up all commands struct dynamic allocation
        Commands *prev = root;
        current = root;
        for (int i = 0; i <= pipes_num; i++) {
            prev = current;
            current = current->next;
            free(prev);
        }
    }
}