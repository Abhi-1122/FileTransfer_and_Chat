#include "pipe.h"
#include <string.h>

int bgpgrp = -1;
int forepgrp = -1;

int count_atoms(atomic *atom)
{
    int n = 0;
    while (atom)
    {
        n++;
        atom = atom->next;
    }
    return n;
}

// Helper: build argv array for exec from atom
char **build_argv(atomic *atom)
{
    int argc = 1;
    if (atom->args)
    {
        // Count spaces for number of args
        for (char *p = atom->args; *p; p++)
        {
            if (*p == ' ')
                argc++;
        }
    }
    char **argv = malloc((argc + 2) * sizeof(char *));
    argv[0] = atom->command;
    int idx = 1;
    if (atom->args)
    {
        char *args_copy = strdup(atom->args);
        char *tok = strtok(args_copy, " ");
        while (tok)
        {
            argv[idx++] = tok;
            tok = strtok(NULL, " ");
        }
        argv[idx] = NULL;
        // Note: do not free args_copy, as argv[] points into it
    }
    else
    {
        argv[1] = NULL;
    }
    return argv;
}

int run_atomic(atomic *atom, int background)
{
    int saved_stdin = -1, saved_stdout = -1;
    int ret = 0;
    // Handle redirection
    if (atom->redirection == 1)
    { // input
        int fd = open_file_for_read(atom->input_file);
        if (fd == -1)
        {
            printf("No such file or directory\n");
            return -1;
        }
        saved_stdin = redirect_stdin(fd);
        close_file(fd);
    }
    else if (atom->redirection == 2)
    { // output
        int fd = open_file_for_write(atom->output_file);
        if (fd == -1)
        {
            printf("Cannot open file for writing\n");
            return -1;
        }
        saved_stdout = redirect_stdout(fd);
        close_file(fd);
    }
    else if (atom->redirection == 3)
    { // append
        int fd = open_file_for_append(atom->output_file);
        if (fd == -1)
        {
            printf("Cannot open file for appending\n");
            return -1;
        }
        saved_stdout = redirect_stdout(fd);
        close_file(fd);
    }
    else if (atom->redirection == 4)
    { // write
        int fdw = open_file_for_write(atom->output_file);
        if (fdw == -1)
        {
            printf("Cannot open file for writing\n");
            return -1;
        }
        int fdi = open_file_for_read(atom->input_file);
        if (fdi == -1)
        {
            printf("No such file or directory\n");
            return -1;
        }
        saved_stdin = redirect_stdin(fdi);
        saved_stdout = redirect_stdout(fdw);
        close_file(fdw);
        close_file(fdi);
    }
    else if (atom->redirection == 5)
    { // append
        int fda = open_file_for_append(atom->output_file);
        if (fda == -1)
        {
            printf("Cannot open file for appending\n");
            return -1;
        }
        int fdi = open_file_for_read(atom->input_file);
        if (fdi == -1)
        {
            printf("No such file or directory\n");
            return -1;
        }
        saved_stdin = redirect_stdin(fdi);
        saved_stdout = redirect_stdout(fda);
        close_file(fda);
        close_file(fdi);
    }

    // Builtin or external
    if (strcmp(atom->command, "hop") == 0)
    {
        if (atom->args)
            ret = hop_command(atom->args);
        else
            ret = hop_command(";");
    }
    else if (strcmp(atom->command, "reveal") == 0)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = getpid();
                setpgid(0, 0);
            }
            if (!atom->args)
                atom->args = ".";
            ret = reveal_command(atom->args);
            // Exit with the actual result from the pipeline
            exit(ret == 0 ? 0 : 1);
        }
        else if (pid > 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = pid;
                setpgid(pid, pid);
                tcsetpgrp(STDIN_FILENO, pid);
                add_process(pid, atom->command, 0);
                int status;
                waitpid(pid, &status, WUNTRACED);
                if (WIFSTOPPED(status))
                {
                    for (int i = 0; i < job_number; i++)
                    {
                        if (process_list[i].pid == pid)
                        {
                            process_list[i].state = 0;
                            process_list[i].background = 1;
                            printf("[%d] Stopped %s\n", process_list[i].job_number, process_list[i].command_name);
                            break;
                        }
                    }
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
                forepgrp = -1;
            }
            else
            {
                wait(NULL);
            }
            // printf("[%d] %d\n", job_number, pid);
        }
        else
        {
            perror("fork");
        }
    }
    else if (strcmp(atom->command, "log") == 0)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = getpid();
                setpgid(0, 0);
            }
            char *argv[32];
            int argc = 0;
            argv[argc++] = "log";
            if (atom->args)
            {
                char *args_copy = strdup(atom->args);
                char *tok = strtok(args_copy, " ");
                while (tok && argc < 31)
                {
                    argv[argc++] = tok;
                    tok = strtok(NULL, " ");
                }
                argv[argc] = NULL;
            }
            else
            {
                argv[argc] = NULL;
            }
            ret = log_command(argv + 1, argc - 1);
            // Exit with the actual result from the pipeline
            exit(ret == 0 ? 0 : 1);
        }
        else if (pid > 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = pid;
                setpgid(pid, pid);
                tcsetpgrp(STDIN_FILENO, pid);
                add_process(pid, atom->command, 0);
                int status;
                waitpid(pid, &status, WUNTRACED);
                if (WIFSTOPPED(status))
                {
                    for (int i = 0; i < job_number; i++)
                    {
                        if (process_list[i].pid == pid)
                        {
                            process_list[i].state = 0;
                            process_list[i].background = 1;
                            printf("[%d] Stopped %s\n", process_list[i].job_number, process_list[i].command_name);
                            break;
                        }
                    }
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
                forepgrp = -1;
            }
            else
            {
                wait(NULL);
            }
            // printf("[%d] %d\n", job_number, pid);
        }
        else
        {
            perror("fork");
        }
    }
    else if (strcmp(atom->command, "activities") == 0)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = getpid();
                setpgid(0, 0);
            }
            ret = activities_command(NULL, 0);
            // Exit with the actual result from the pipeline
            exit(ret == 0 ? 0 : 1);
        }
        else if (pid > 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = pid;
                setpgid(pid, pid);
                tcsetpgrp(STDIN_FILENO, pid);
                int status;
                waitpid(pid, &status, WUNTRACED);
                if (WIFSTOPPED(status))
                {
                    for (int i = 0; i < job_number; i++)
                    {
                        if (process_list[i].pid == pid)
                        {
                            process_list[i].state = 0;
                            process_list[i].background = 1;
                            printf("[%d] Stopped %s\n", process_list[i].job_number, process_list[i].command_name);
                            break;
                        }
                    }
                }
                tcsetpgrp(STDIN_FILENO, getpgrp());
                forepgrp = -1;
            }
            else
            {
                wait(NULL);
            }
            // printf("[%d] %d\n", job_number, pid);
        }
        else
        {
            perror("fork");
        }
    }
    else if (strcmp(atom->command, "ping") == 0)
    {
        ret = ping(atom->args);
    }
    else if (strcmp(atom->command, "fg") == 0)
    {
        ret = fg(atom->args);
    }
    else if (strcmp(atom->command, "bg") == 0)
    {
        ret = bg(atom->args);
    }
    else if (strcmp(atom->command, "exit") == 0)
    {
        exit(0);
    }
    else
    {
        // External command
        char **argv = build_argv(atom);
        // for (int i = 0; argv[i] != NULL; i++)
        // {
        //     printf("argv[%d]: %s\n", i, argv[i]);
        // }
        pid_t pid = fork();
        if (pid == 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = getpid();
                setpgid(0, 0);
            }
            execvp(argv[0], argv);
            perror("execvp failed");
            exit(1);
        }
        else if (pid > 0)
        {
            int status;
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = pid;
                setpgid(pid, pid);
                tcsetpgrp(STDIN_FILENO, pid);

                add_process(pid, atom->command, 0);
                // Wait for foreground process with WUNTRACED to catch Ctrl+Z
                waitpid(pid, &status, WUNTRACED);

                // Check if process was stopped by Ctrl+Z
                if (WIFSTOPPED(status))
                {
                    // Update process state to stopped and background
                    for (int i = 0; i < job_number; i++)
                    {
                        if (process_list[i].pid == pid)
                        {
                            process_list[i].state = 0;      // stopped
                            process_list[i].background = 1; // now background
                            printf("[%d] Stopped %s\n", process_list[i].job_number, process_list[i].command_name);
                            break;
                        }
                    }
                }

                // Restore terminal control to shell
                tcsetpgrp(STDIN_FILENO, getpgrp());
                forepgrp = -1;
            }
            else
            {
                waitpid(pid, &status, 0);
            }
            ret = (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
        }
        free(argv);
    }

    if (saved_stdin != -1)
        restore_stdin(saved_stdin);
    if (saved_stdout != -1)
        restore_stdout(saved_stdout);
    return ret;
}

int run_atomic_bg(atomic *atom, int background)
{
    int saved_stdin = -1, saved_stdout = -1;
    int ret = 0;
    // Handle redirection
    if (atom->redirection == 1)
    { // input
        int fd = open_file_for_read(atom->input_file);
        if (fd == -1)
        {
            printf("No such file or directory\n");
            return -1;
        }
        saved_stdin = redirect_stdin(fd);
        close_file(fd);
    }
    else if (atom->redirection == 2)
    { // output
        int fd = open_file_for_write(atom->output_file);
        if (fd == -1)
        {
            printf("Cannot open file for writing\n");
            return -1;
        }
        saved_stdout = redirect_stdout(fd);
        close_file(fd);
    }
    else if (atom->redirection == 3)
    { // append
        int fd = open_file_for_append(atom->output_file);
        if (fd == -1)
        {
            printf("Cannot open file for appending\n");
            return -1;
        }
        saved_stdout = redirect_stdout(fd);
        close_file(fd);
    }
    else if (atom->redirection == 4)
    { // write
        int fdw = open_file_for_write(atom->output_file);
        if (fdw == -1)
        {
            printf("Cannot open file for writing\n");
            return -1;
        }
        int fdi = open_file_for_read(atom->input_file);
        if (fdi == -1)
        {
            printf("No such file or directory\n");
            return -1;
        }
        saved_stdin = redirect_stdin(fdi);
        saved_stdout = redirect_stdout(fdw);
        close_file(fdw);
        close_file(fdi);
    }
    else if (atom->redirection == 5)
    { // append
        int fda = open_file_for_append(atom->output_file);
        if (fda == -1)
        {
            printf("Cannot open file for appending\n");
            return -1;
        }
        int fdi = open_file_for_read(atom->input_file);
        if (fdi == -1)
        {
            printf("No such file or directory\n");
            return -1;
        }
        saved_stdin = redirect_stdin(fdi);
        saved_stdout = redirect_stdout(fda);
        close_file(fda);
        close_file(fdi);
    }

    // Builtin or external
    if (strcmp(atom->command, "hop") == 0)
    {
        if (atom->args)
            ret = hop_command(atom->args);
        else
            ret = hop_command(";");
    }
    else if (strcmp(atom->command, "reveal") == 0)
    {

        if (!atom->args)
            atom->args = ".";
        ret = reveal_command(atom->args);
    }
    else if (strcmp(atom->command, "log") == 0)
    {
        char *argv[32];
        int argc = 0;
        argv[argc++] = "log";
        if (atom->args)
        {
            char *args_copy = strdup(atom->args);
            char *tok = strtok(args_copy, " ");
            while (tok && argc < 31)
            {
                argv[argc++] = tok;
                tok = strtok(NULL, " ");
            }
            argv[argc] = NULL;
        }
        else
        {
            argv[argc] = NULL;
        }
        ret = log_command(argv + 1, argc - 1);
    }
    else if (strcmp(atom->command, "activities") == 0)
    {
        ret = activities_command(NULL, 0);
    }
    else if (strcmp(atom->command, "ping") == 0)
    {
        ret = ping(atom->args);
    }
    else if (strcmp(atom->command, "fg") == 0)
    {
        ret = fg(atom->args);
    }
    else if (strcmp(atom->command, "bg") == 0)
    {
        ret = bg(atom->args);
    }
    else if (strcmp(atom->command, "exit") == 0)
    {
        ret = 0;
        exit(0);
    }
    else
    {
        // External command
        char **argv = build_argv(atom);
        // for (int i = 0; argv[i] != NULL; i++)
        // {
        //     printf("argv[%d]: %s\n", i, argv[i]);
        // }
        execvp(argv[0], argv);
        perror("execvp failed");
        ret = -1;
        free(argv);
    }

    if (saved_stdin != -1)
        restore_stdin(saved_stdin);
    if (saved_stdout != -1)
        restore_stdout(saved_stdout);
    return ret;
}

int run_pipeline(atomic *atom_head, int background)
{
    int n = count_atoms(atom_head);
    if (n == 1 && background == 0)
        return run_atomic(atom_head, background);
    if (n == 1 && background == 1)
        return run_atomic_bg(atom_head, background);

    int pipes[n - 1][2];
    atomic *atom = atom_head;
    pid_t pids[n];
    for (int i = 0; i < n - 1; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe failed");
            exit(1);
        }
    }
    int i = 0;
    while (atom)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = getpid();
                setpgid(0, forepgrp);
            }

            if (i > 0)
            {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < n - 1)
            {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            for (int j = 0; j < n - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            if (atom->redirection == 1)
            {
                int fd = open_file_for_read(atom->input_file);
                if (fd != -1)
                {
                    dup2(fd, STDIN_FILENO);
                    close_file(fd);
                }
            }
            else if (atom->redirection == 2)
            {
                int fd = open_file_for_write(atom->output_file);
                if (fd != -1)
                {
                    dup2(fd, STDOUT_FILENO);
                    close_file(fd);
                }
            }
            else if (atom->redirection == 3)
            {
                int fd = open_file_for_append(atom->output_file);
                if (fd != -1)
                {
                    dup2(fd, STDOUT_FILENO);
                    close_file(fd);
                }
            }
            if (strcmp(atom->command, "hop") == 0)
            {
                exit(hop_command(atom->args ? atom->args : ";"));
            }
            else if (strcmp(atom->command, "reveal") == 0)
            {
                if (!atom->args)
                    atom->args = ".";
                exit(reveal_command(atom->args));
            }
            else if (strcmp(atom->command, "log") == 0)
            {
                char *argv[32];
                int argc = 0;
                argv[argc++] = "log";
                if (atom->args)
                {
                    char *args_copy = strdup(atom->args);
                    char *tok = strtok(args_copy, " ");
                    while (tok && argc < 31)
                    {
                        argv[argc++] = tok;
                        tok = strtok(NULL, " ");
                    }
                    argv[argc] = NULL;
                }
                else
                {
                    argv[argc] = NULL;
                }
                exit(log_command(argv + 1, argc - 1));
            }
            else if (strcmp(atom->command, "activities") == 0)
            {
                exit(activities_command(NULL, 0));
            }
            else if (strcmp(atom->command, "ping") == 0)
            {
                exit(ping(atom->args));
            }
            else if (strcmp(atom->command, "exit") == 0)
            {
                exit(0);
            }
            else
            {
                char **argv = build_argv(atom);
                // for (int i = 0; argv[i] != NULL; i++)
                // {
                //     printf("argv[%d]: %s\n", i, argv[i]);
                // }
                int ret = execvp(argv[0], argv);
                // printf("%d\n", ret);
                if (ret == -1)
                {
                    perror("execvp failed");
                    exit(1);
                }
                else
                {
                    fflush(stdout);
                    // printf("exiting tail\n");
                    exit(0);
                }
            }
        }
        else if (pid > 0)
        {
            pids[i] = pid;
            if (background == 0)
            {
                if (forepgrp == -1)
                    forepgrp = pid;
                setpgid(pid, forepgrp);
            }
            // if(background==1)
            // printf(" %d", pid);
            if (i == 0 && background == 0)
            {
                char buf[256] = "pipeline(";
                strcat(buf, atom->command);
                strcat(buf, ")");
                add_process(pid, buf, background);
            }
        }
        atom = atom->next;
        i++;
    }
    for (int j = 0; j < n - 1; j++)
    {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }
    // Wait for all children
    if (background == 0 && forepgrp != 1)
    {
        tcsetpgrp(STDIN_FILENO, forepgrp);
    }
    int status = 0;
    for (int j = 0; j < n; j++)
    {
        if (background == 0)
        {
            waitpid(pids[j], &status, WUNTRACED);
            if (WIFSTOPPED(status))
            {
                // Handle stopped processes
                for (int i = 0; i < job_number; i++)
                {
                    if (process_list[i].pid == pids[j])
                    {
                        process_list[i].state = 0;      // stopped
                        process_list[i].background = 1; // now background
                        printf("[%d] Stopped %s\n", process_list[i].job_number, process_list[i].command_name);
                        break;
                    }
                }
            }
        }
        else
        {
            waitpid(pids[j], &status, 0);
        }
    }
    if (background == 0)
    {
        tcsetpgrp(STDIN_FILENO, getpgrp());
        forepgrp = -1;
    }
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

void run_command(cmd_group *curr)
{
    int last_status = 0;
    while (curr)
    {
        // Handle background
        if (curr->background)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                if (bgpgrp == -1)
                    bgpgrp = getpid();
                setpgid(0, 0);

                // int null_fd = open("/dev/null", O_RDONLY);
                // if (null_fd != -1)
                // {
                //     dup2(null_fd, STDIN_FILENO);
                //     close(null_fd);
                // }

                int pipeline_result = run_pipeline(curr->atom, 1);
                // Exit with the actual result from the pipeline
                exit(pipeline_result == 0 ? 0 : 1);
            }
            else if (pid > 0)
            {
                if (bgpgrp == -1)
                    bgpgrp = pid;
                setpgid(pid, pid);
                if (count_atoms(curr->atom) > 1)
                {
                    char buf[256] = "pipeline(";
                    strcat(buf, curr->atom->command);
                    strcat(buf, ")");
                    int job_number = add_process(pid, buf, 1);
                    printf("[%d] %d\n", job_number, pid);
                }
                else
                {
                    int job_number = add_process(pid, curr->atom->command, 1);
                    printf("[%d] %d\n", job_number, pid);
                }
            }
            else
            {
                perror("fork");
            }
        }
        else
        {
            last_status = run_pipeline(curr->atom, 0);
        }
        // Handle && and ;
        if (curr->and && last_status != 0)
            break;
        curr = curr->next;
    }
}