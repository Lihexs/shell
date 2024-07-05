#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int prepare(void);
int process_arglist(int count, char **arglist);
int finalize(void);
int find_pipe_index(int count, char **arglist);
int execute_command(int count, char **arglist, int background_running);

int find_pipe_index(int count, char **arglist)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(arglist[i], "|") == 0)
            return i;
    }
    return -1;
}

void reap_child_processes()
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
    }
}
int prepare(void)
{

    struct sigaction newAction =
        {
            .sa_handler = SIG_IGN,
            .sa_flags = SA_RESTART};

    if (sigaction(SIGINT, &newAction, NULL) == -1)
    {
        perror("Signal handle registration failed");
        return 1;
    }
    return 0;
}

int execute_command(int count, char **arglist, int background_running)
{
    reap_child_processes();
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("Fork failed");
        return 1;
    }
    else if (pid == 0)
    {
        if (count > 2 && strcmp(arglist[count - 2], "<") == 0)
        {
            int fd = open(arglist[count - 1], O_RDONLY);
            if (fd == -1)
            {
                perror("Failed to open input file");
                exit(1);
            }
            if (dup2(fd, STDIN_FILENO) == -1)
            {
                perror("Failed to redirect input");
                close(fd);
                exit(1);
            }
            close(fd);
            arglist[count - 2] = NULL;
            count -= 2;
        }

        if (count > 2 && strcmp(arglist[count - 2], ">>") == 0)
        {
            int fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd == -1)
            {
                perror("Failed to open output file");
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) == -1)
            {
                perror("Failed to redirect output");
                close(fd);
                exit(1);
            }
            close(fd);
            arglist[count - 2] = NULL;
            count -= 2;
        }

        if (execvp(arglist[0], arglist) == -1)
        {
            perror("Execution failed");
            exit(1);
        }
    }
    else
    {
        if (!background_running)
        {
            int status;
            if (waitpid(pid, &status, 0) == -1)
            {
                perror("waitpid failed");
                return 1;
            }
        }
    }
    return 1;
}

int process_arglist(int count, char **arglist)
{
    reap_child_processes();
    if (count == 0 || arglist[0] == NULL)
    {
        return 1;
    }
    int background_running = 0;
    int pipe_index = -1;

    if (count > 0 && strcmp(arglist[count - 1], "&") == 0)
    {
        background_running = 1;
        arglist[count - 1] = NULL;
        count--;
    }

    pipe_index = find_pipe_index(count, arglist);

    if (pipe_index > -1)
    {
        int pipefd[2];
        if (pipe(pipefd) == -1)
        {
            perror("Pipe failed");
            return 1;
        }

        pid_t pid1 = fork();
        if (pid1 == -1)
        {
            perror("Fork failed");
            return 1;
        }
        else if (pid1 == 0)
        {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            arglist[pipe_index] = NULL;
            execvp(arglist[0], arglist);
            perror("Execution failed");
            exit(1);
        }

        pid_t pid2 = fork();
        if (pid2 == -1)
        {
            perror("Fork failed");
            return 1;
        }
        else if (pid2 == 0)
        {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);

            execvp(arglist[pipe_index + 1], &arglist[pipe_index + 1]);
            perror("Execution failed");
            exit(1);
        }
        close(pipefd[0]);
        close(pipefd[1]);

        int status1, status2;
        waitpid(pid1, &status1, 0);
        waitpid(pid2, &status2, 0);

        return 1;
    }

    return execute_command(count, arglist, background_running);
}

int finalize(void)
{
    struct sigaction newAction =
        {
            .sa_handler = SIG_DFL,
            .sa_flags = 0};

    if (sigaction(SIGINT, &newAction, NULL) == -1)
    {
        perror("Signal handle restoration failed");
        return 1;
    }
    return 0;
}