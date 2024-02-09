#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
pid_t pid;
int process_command_in_background(char **arglist, int count);
int process_pipeing(char **arglist, int count, int separator);
int process_input_redirection(char **arglist, int count, int separator);
int process_output_redirection(char **arglist, int count, int separator);
int process_command(char **arglist);

void setup_child_sigaction() {
    struct sigaction sa_child;
    sa_child.sa_handler =  SIG_DFL; 
    sigaction(SIGINT, &sa_child, NULL);
}


int prepare(void){
    struct sigaction sa_child,sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa_ignore, NULL);
    sa_child.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa_child, NULL);
    return 0;
}


int handle_childDone(pid_t pid){
    if (pid == -1){
        if (errno == EINTR || errno == ECHILD){
            return 1;
        }
        perror("error was in the child");
        exit(EXIT_FAILURE);
    }
    return 1;
}


int process_command(char ** arglist){
    pid = fork();
    if (pid < 0){
        perror("Failed forking");
        exit(EXIT_FAILURE);
    }
    if (pid == 0){
        setup_child_sigaction();
        execvp(arglist[0],arglist);
        perror("Failed execvp");
        exit(EXIT_FAILURE);
    }
    else{
        int status;
        pid = waitpid(pid, &status, 0);
        return handle_childDone(pid);
    }
}


int process_command_in_background(char ** arglist,int count){
    pid_t pid = fork();
    if (pid < 0){
        perror("Failed forking");
        exit(EXIT_FAILURE);
    }
    if (pid == 0){
        struct sigaction sa_child;
        sa_child.sa_handler = SIG_DFL;
        sigaction(SIGCHLD, &sa_child, NULL);
        char **command_args = &arglist[0];
        command_args[count-1] = NULL;
        execvp(command_args[0],command_args);
        perror("Failed execvp");
        exit(EXIT_FAILURE);
    }
    return 1;
}

int process_output_redirection(char ** arglist,int count,int separator){
    char *file_name = arglist[separator+1];
    pid = fork();
    if (pid == -1) {
        perror("Failed forking");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        setup_child_sigaction();
        FILE *file = fopen(file_name,"w");
        if (file == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        if(dup2(fileno(file),STDOUT_FILENO) == -1){
            perror("Failed dup2");
            exit(EXIT_FAILURE);          
        }  
        fclose(file);
        char **command_args = &arglist[0];
        command_args[separator] = NULL;
        execvp(command_args[0],command_args);
        perror("Failed execvp");
        exit(EXIT_FAILURE);  
    }
    int status;
    pid = waitpid(pid, &status, 0);
    return handle_childDone(pid);
}

int process_input_redirection(char ** arglist,int count,int separator){
    char *file_name = arglist[separator+1];
    pid = fork();
    if (pid == -1) {
        perror("Failed forking");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        FILE *file = fopen(file_name,"r");
        if (file == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        setup_child_sigaction();
        if(dup2(fileno(file),STDIN_FILENO) == -1){
            perror("Failed dup2");
            exit(EXIT_FAILURE);
        }
        fclose(file);
        char **command_args = &arglist[0];
        command_args[separator] = NULL;
        execvp(command_args[0],command_args);
        exit(EXIT_FAILURE);
    }
    int status;
    pid = waitpid(pid, &status, 0);
    return handle_childDone(pid);
}
int process_pipeing(char ** arglist,int count,int separator){
    int pipes[2];
    int status;
    if(-1 == pipe(pipes)){
        perror("pipe");
        exit(-1);
    }
    pid_t pidFirst = fork();
    if(pidFirst == -1){
        perror("Failed forking");
        close(pipes[0]);
        close(pipes[1]);
        exit(EXIT_FAILURE);
    }
    else if (pidFirst == 0)
    {
        setup_child_sigaction();
        close(pipes[0]);
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[1]);
        char **command_args = &arglist[0];
        command_args[separator] = NULL;
        if (execvp(command_args[0], command_args) == -1) {
            perror("Failed execvp");
            close(pipes[0]);
            close(pipes[1]);
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS); 
    }
    pid = fork();
    if (pid == -1) {
        perror("Failed forking");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) 
    {
        setup_child_sigaction();
        close(pipes[1]);
        dup2(pipes[0], STDIN_FILENO);
        close(pipes[0]);
        char **command_args = &arglist[separator+1];
        if (execvp(command_args[0], command_args) == -1) {
            perror("Failed execvp");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS); 
    }
    else {
        close(pipes[0]);
        close(pipes[1]);
        pidFirst = waitpid(pidFirst, &status, 0);
        pid = waitpid(pid, &status, 0);
        return handle_childDone(pidFirst) || handle_childDone(pid);
    }
}

int finalize() {
    while (waitpid(-1,NULL,0)>0);
    return 0;
}


int process_arglist(int count, char **arglist){
    int i ;
    if (strcmp(arglist[count-1], "&") == 0 )
        return process_command_in_background(arglist,count);
    for(i = 0;i<count;i++){  
        if (strcmp(arglist[i], "|") == 0)
            return process_pipeing(arglist,count,i);
        else if (strcmp(arglist[i], "<") == 0)
            return process_input_redirection(arglist,count,i);
        else if (strcmp(arglist[i], ">" ) == 0)
            return process_output_redirection(arglist,count,i);
    }
    return process_command(arglist);
}



