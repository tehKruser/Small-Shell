#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>

#define COMMAND_MAX		2048
#define ARGS_MAX		512
#define STRING_MAX		256

/* Global Variables */
int ChldFlag = 0;
int PreventBGFlag = 0;
int BGFlag = 0;

/* Function prototypes */
int getArguments(char** argsArray, int* fileIO);
void changeDirectory(char** argsArray);
void forkandexec(char ** argsArray, int* statusArray, int* fileIO);
void getLastStatus(int *statusArray);
void catchSIGTSTP(int signo);
void catchSIGCHLD(int signo);
void catchSIGUSR1(int signo);


/* shell */
void main()
{
    /* Signal handling - set up */
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, SIGCHLD_action = {0}, SIGUSR1_action = {0};

    SIGINT_action.sa_handler = SIG_IGN;

    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;

    SIGUSR1_action.sa_handler = catchSIGUSR1;
    sigfillset(&SIGUSR1_action.sa_mask);
    SIGUSR1_action.sa_flags = 0;

    SIGCHLD_action.sa_handler = catchSIGCHLD;
    sigfillset(&SIGCHLD_action.sa_mask);
    SIGCHLD_action.sa_flags = SA_RESTART;

    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    sigaction(SIGUSR1, &SIGUSR1_action, NULL);
    sigaction(SIGCHLD, &SIGCHLD_action, NULL);

    /* Array for command line arguments */
    char* command_args[ARGS_MAX];
    memset(command_args, 0, sizeof(command_args[0]) * ARGS_MAX);

    /* status[0]: 0 is exit value, 1 is terminated by signal, status[1]: value of status */
    int status[2];
    memset(status, 0, sizeof(status[0]) * 2);


    /* fileIO[0] = file in, fileIO[1] = file out, -5 indicates no file descriptor */
    int fileIO[2];
    fileIO[0] = -5;
    fileIO[1] = -5;

    while(getArguments(command_args, fileIO)){              // runs while getArguments returns 1

        if(command_args[0] != NULL) {                       // Comment found if NULL
            if(strstr(command_args[0], "#") == NULL){
                /* Command: cd */
                if(strcmp(command_args[0], "cd") == 0){
                    changeDirectory(command_args);
                }
                /* Command: status */
                else if(strcmp(command_args[0], "status") == 0){
                    getLastStatus(status);
                }
                /* exec commands */
                else {
                    // fork and execute function
                    forkandexec(command_args, status, fileIO);
                }
            }

            /* free up memory */
            int i = 0;
            while(command_args[i] != NULL){
                free(command_args[i]);
                command_args[i] = NULL;
                i++;
            }
        }
    }

    /* Handle exit */
    exit(0);
}


int getArguments(char** argsArray, int* fileIO){

    size_t bufferSize = COMMAND_MAX-1;
    char lineEntered[COMMAND_MAX];
    memset(lineEntered, '\0', sizeof(char) * COMMAND_MAX);
    char* command;
    int numCharEntered = -5;
    int no_exit = 1;
    bufferSize = COMMAND_MAX-1;

    /* Check for completed background processes */
    pid_t pid;
    int status;

    /* Check if ChldFlag has been set due to background process completion */
    if(ChldFlag == 1){
        raise(SIGUSR1);     // SIGURSR1 handler used to reap children
    }

    /* Command line */
    printf(": ");
    fflush(stdout);

    fflush(stdin);
    if(fgets(lineEntered, bufferSize, stdin) != NULL){
        lineEntered[strlen(lineEntered)-1] = '\0';          // remove newline character
    }

    numCharEntered = strlen(lineEntered);

    /* check number of characters in command line against max allowable */
    if(numCharEntered > COMMAND_MAX){
        printf("Command line exceeded maximum length\n");
        fflush(stdout);
    }
    else if(lineEntered != NULL){
        char* token = strtok(lineEntered, "  \t");  //get first command

        if(token != NULL){
            if(strstr(token, "#") != NULL){         // comment found
                token = NULL;
            }
        }

        int arg = 0;
        if(token != NULL){                          // if not a comment
            if(strcmp(token, "exit") == 0){         // check for exit command
                no_exit = 0;                        // 0 indicates user is done with shell
            } else {
                /* loop through command string for arguments */
                while(token != NULL){
                    if(arg > ARGS_MAX){                         // check against maximum number of arguments
                        printf("Exceeded maximum allowable arguments!\n");
                        fflush(stdout);
                    } else {
                        /* file input indication found */
                        if(strcmp(token, "<") == 0 && PreventBGFlag != 1){
                            token = strtok(NULL, "  \t");
                            fileIO[0] = open(token, O_RDONLY);
                            if(fileIO[0] == -1){
                                printf("cannot open %s for input\n", token);
                                fflush(stdout);
                            }
                        }
                        /* file output indication found */
                        else if(strcmp(token, ">") == 0){
                            token = strtok(NULL, "  \t");
                            fileIO[1] = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            if(fileIO[1] == -1){
                                printf("cannot open %s for output\n", token);
                                fflush(stdout);
                            }
                        }
                        else if(strcmp(token, "&") == 0){
                            /* flag for background process if '&' is end of string */
                            token = strtok(NULL, "  \t");
                            if(token == NULL){
                                if(PreventBGFlag == 0){
                                    BGFlag = 1;             // set BGFlag if BG processes are OK

                                    /* set stdin and stdout if not already specified */
                                    if(fileIO[0] == -5){
                                        fileIO[0] = open("/dev/null", O_RDONLY);
                                        if(fileIO[0] == -1){
                                            printf("cannot open dev/null for input\n");
                                            fflush(stdout);
                                        }
                                    }
                                    if(fileIO[1] == -5){
                                        fileIO[1] = open("/dev/null", O_WRONLY);
                                        if(fileIO[1] == -1){
                                            printf("cannot open dev/null for output\n");
                                            fflush(stdout);
                                        }
                                    }
                                } else {
                                    BGFlag = 0;         // reset BGFlag if BG processes not OK
                                }
                            } else {
                                /* treat '&' as a character for the argument */
                                command = malloc(STRING_MAX * sizeof(char));
                                assert(command != 0);
                                memset(command, '\0', STRING_MAX);
                                strcpy(command, "&");
                                argsArray[arg] = command;
                                arg++;
                            }

                        } else {
                            /* token doesn't match built in commands or a comment - treat as valid argument */
                            command = malloc(STRING_MAX * sizeof(char));
                            assert(command != 0);
                            memset(command, '\0', STRING_MAX);

                            /* variable expansion on '$$ occurrences '*/
                            if(strcmp(token, "$$") == 0){                   // $$ by itself on command line
                                sprintf(command, "%ld", (long)getpid());
                            } else if(strstr(token, "$$") != NULL){         // $$ attached to a string
                                int terminalLocation = strstr(token, "$$") - token;
                                strcpy(command, token);
                                command[terminalLocation] = '\0';
                                sprintf(command, "%s%ld", command, (long)getpid());
                            } else {
                                strcpy(command, token);                 // copy token to command
                            }

                            argsArray[arg] = command;                   // add command to argument list
                            arg++;
                        }
                    }
                    if(token != NULL){
                       token = strtok(NULL, "  \t");                       // get next token in command line input
                    }
                }
            }
        }
    }
    return no_exit;         // 1 - keep running shell, 0 - leave shell
}


/* changes directory - HOME used if no arguments provided */
void changeDirectory(char** argsArray){
    if(argsArray[1] == NULL){
        const char* s = getenv("HOME");
        chdir(s);
    } else {
        int dirOK = chdir(argsArray[1]);
        if(dirOK != 0){
            printf("No such directory\n");
        }
    }
}


/* Returns the status of the last operation */
void getLastStatus(int *statusArray){
    if(statusArray[0] == 0){
        printf("exit value %d\n", statusArray[1]);
        fflush(stdout);
    } else {
        printf("terminated by signal %d\n", statusArray[1]);
        fflush(stdout);
    }
}


/* Execute non-built in commands */
void forkandexec(char** argsArray, int* statusArray, int* fileIO){
    /* Check if valid I/O */
    if(fileIO[0] != -1 && fileIO[1] != -1){
        pid_t spawnPid = -5;
        int childExitStatus = -5;
        spawnPid = fork();
        int result;
        switch (spawnPid) {
            case -1: { perror("Hull Breach!\n"); exit(1); break; }
            case 0: {
                struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};
                /* Ignore SIGTSTP on child processes */
                SIGTSTP_action.sa_handler = SIG_IGN;
                sigaction(SIGTSTP, &SIGTSTP_action, NULL);

                /* Ignore SIGINT if child is background process, else use default handler */
                if(BGFlag == 1 && PreventBGFlag == 0){
                    SIGINT_action.sa_handler = SIG_IGN;
                } else {
                    SIGINT_action.sa_handler = SIG_DFL;
                }
                sigaction(SIGINT, &SIGINT_action, NULL);

                /* set input and output */
                if(fileIO[0] != -5){
                    result = dup2(fileIO[0], 0);
                }
                if(fileIO[1] != -5){
                    result = dup2(fileIO[1], 1);
                }

                /* execute command */
                execvp(*argsArray, argsArray);
                perror(*argsArray);
                exit(2); break;
            }
            default: {
                /* If BG process, display PID and reset flag */
                if(BGFlag == 1 && PreventBGFlag == 0){
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);
                    BGFlag = 0;
                } else {
                    sigset_t x;
                    sigemptyset (&x);
                    sigaddset(&x, SIGTSTP);
                    sigprocmask(SIG_BLOCK, &x, NULL);           // Block SIGTSTP signals until waitpid finishes
                    pid_t actualPid = -1;

                    actualPid = waitpid(spawnPid, &childExitStatus, 0);

                    if(WIFEXITED(childExitStatus)){
                        // exited normally
                        statusArray[0] = 0;
                        statusArray[1] = WEXITSTATUS(childExitStatus);
                    } else if(WIFSIGNALED(childExitStatus)){
                        // terminated by signal
                        statusArray[0] = 1;
                        statusArray[1] = WTERMSIG(childExitStatus);
                        printf("terminated by signal %d\n", statusArray[1]);
                        fflush(stdout);
                    }
                    sigprocmask(SIG_UNBLOCK, &x, NULL);     // unblock SIGTSTP signals
                }
                break;
            }
        }
    } else {
        /* Reset statuses */
        statusArray[0] = 0;
        statusArray[1] = 1;
        fileIO[0] = -5;
        fileIO[1] = -5;
        BGFlag = 0;
    }

    /* close input and output */
    if(fileIO[0] != -5){
        close(fileIO[0]);
        fileIO[0] = -5;
    }
    if(fileIO[1] != -5){
        close(fileIO[1]);
        fileIO[1] = -5;
    }
}


/* catch SIGCHLD and set flag to indicate a child is ready to be reaped */
void catchSIGCHLD(int signo){
    ChldFlag = 1;
}


/* catch SIGTSTP, display messages and set flags */
void catchSIGTSTP(int signo){
    if(PreventBGFlag == 0){
        PreventBGFlag = 1;
        BGFlag = 0;
        char* message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
    } else {
        PreventBGFlag = 0;
        BGFlag = 0;
        char* message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
    }
}


/*
    Signal Handler for SIGUSR1 - used to reap background children.
    Advantage of using handler is to block any other signals while children are being reaped.
 */
void catchSIGUSR1(int signo){
    int childExitStatus = -5;
    pid_t actualPid;

    /*
        if a child is reaped, message will print as:
        background pid X is done: exit value Y
        or
        background pid X is done: terminated by signal Y
    */
    while((actualPid = waitpid(-1, &childExitStatus, WNOHANG)) > 0){
        char* message1 = "background pid ";
        write(STDOUT_FILENO, message1, 15);

        char pidstr[10];
        memset(pidstr, '\0', sizeof(char)*10);
        sprintf(pidstr, "%d", actualPid);
        write(STDOUT_FILENO, pidstr, 10);

        char* message3 = " is done: ";
        write(STDOUT_FILENO, message3, 10);

        if(WIFEXITED(childExitStatus)){
            // exited normally
            char* message4 = "exit value ";
            write(STDOUT_FILENO, message4, 11);

            char statusChar[3];
            memset(statusChar, '\0', sizeof(char)*3);
            sprintf(statusChar, "%d", WEXITSTATUS(childExitStatus));
            write(STDOUT_FILENO, statusChar, 3);
        } else {
            // terminated by signal
            char* message4 = "terminated by signal ";
            write(STDOUT_FILENO, message4, 21);

            char statusChar[3];
            memset(statusChar, '\0', sizeof(char)*3);
            sprintf(statusChar, "%d", WTERMSIG(childExitStatus));
            write(STDOUT_FILENO, statusChar, 3);
        }

        char* newLine = "\n";
        write(STDOUT_FILENO, newLine, 1);

    }
    ChldFlag = 0;       // reset ChldFlag
}
