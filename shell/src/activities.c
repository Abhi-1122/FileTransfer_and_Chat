#include "activities.h"
#include "file.h"

// Global process list
process_info_t process_list[MAX_PROCESSES];
int job_number = 0;

void init_process_list(void)
{
    job_number = 0;
}

int add_process(pid_t pid, const char *command_name, int background)
{
    if (job_number >= MAX_PROCESSES)
    {
        return -1; // Process list is full
    }

    process_list[job_number].job_number = job_number + 1;
    process_list[job_number].pid = pid;
    strncpy(process_list[job_number].command_name, command_name, sizeof(process_list[job_number].command_name) - 1);
    process_list[job_number].command_name[sizeof(process_list[job_number].command_name) - 1] = '\0';
    process_list[job_number].state = 1;
    process_list[job_number].background = background;
    job_number++;

    return job_number;
}

// void remove_process(pid_t pid) {
//     for (int i = 0; i < process_count; i++) {
//         if (process_list[i].pid == pid && process_list[i].active) {
//             process_list[i].active = 0;
//             break;
//         }
//     }
// }
// ########## LLM GENERATED CODE BEGINS ##########
void update_process_states(void)
{
    for (int i = 0; i < job_number; i++)
    {
        if (process_list[i].state == 2)
        {
            continue;
        }

        // Check if process still exists and get its state
        char proc_path[256];
        char state_buffer[256];
        snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", process_list[i].pid);

        FILE *stat_file = fopen(proc_path, "r");
        if (stat_file == NULL)
        {
            // Process no longer exists
            process_list[i].state = 2;
            continue;
        }

        // Read the stat file to get process state
        if (fgets(state_buffer, sizeof(state_buffer), stat_file) != NULL)
        {
            // printf("Process %d state %s: ", process_list[i].pid,state_buffer);
            // Parse the stat file - state is the 3rd field
            char *token = strtok(state_buffer, " ");
            for (int j = 0; j < 2 && token != NULL; j++)
            {
                token = strtok(NULL, " ");
            }

            if (token != NULL)
            {
                char state_char = token[0];
                switch (state_char)
                {
                case 'R':
                case 'S':
                case 'D':
                    process_list[i].state = 1;
                    break;
                case 'T':
                    process_list[i].state = 0;
                    break;
                case 'Z':
                    process_list[i].state = 2;
                    break;
                default:
                    process_list[i].state = 1;
                    break;
                }
            }
        }

        fclose(stat_file);
    }
}
// ########## LLM GENERATED CODE ENDS ##########
void check_background_jobs(void)
{
    int found_completed = 0;
    // update_process_states();

    for (int i = 0; i < job_number; i++)
    {
        if (process_list[i].background == 0)
            continue;
        if (process_list[i].state != 1)
        {
            continue;
        }

        int status;
        pid_t result = waitpid(process_list[i].pid, &status, WNOHANG | WUNTRACED);

        if (result > 0)
        {
            if (!found_completed)
            {
                found_completed = 1;
            }

            // Check if process exited normally AND with exit status 0
            if (WIFEXITED(status))
            {
                int exit_code = WEXITSTATUS(status);
                if (exit_code == 0)
                {
                    printf("%s with pid %d exited normally\n",
                           process_list[i].command_name,
                           process_list[i].pid);
                }
                else
                {
                    printf("%s with pid %d exited abnormally with code %d\n",
                           process_list[i].command_name,
                           process_list[i].pid,
                           exit_code);
                }
                process_list[i].state = 2;
            }
            else if (WIFSTOPPED(status))
            {
                printf("%s with pid %d stopped\n",
                       process_list[i].command_name,
                       process_list[i].pid);
                process_list[i].state = 0;
                process_list[i].background = 1;
            }
            else
            {
                // Process was terminated by signal or other abnormal condition
                printf("%s with pid %d exited abnormally\n",
                       process_list[i].command_name,
                       process_list[i].pid);
            }

            // Remove from activities and background jobs
            // remove_process(background_jobs[i].pid);
            // process_list[i].state = 2;
        }
        else if (result == -1)
        {
            printf("Process %d no longer exists\n", process_list[i].pid);
            process_list[i].state = 2;
            // remove_process(background_jobs[i].pid);
        }
    }

    if (found_completed)
    {
        fflush(stdout);
    }
}

int compare_processes(const void *a, const void *b)
{
    const process_info_t *proc_a = (const process_info_t *)a;
    const process_info_t *proc_b = (const process_info_t *)b;
    return strcmp(proc_a->command_name, proc_b->command_name);
}

int activities_command(char **tokens, int token_count)
{
    // Update process states before displaying
    update_process_states();
    // cleanup_terminated_processes();
    // printf("Active processes:\n");
    if (job_number == 0)
    {
        printf("No processes found.\n");
        return 0; // No processes to display
    }

    // Create a copy of active processes for sorting
    process_info_t *active_processes = malloc(job_number * sizeof(process_info_t));
    if (active_processes == NULL)
    {
        printf("Memory allocation failed\n");
        return -1;
    }

    int active_count = 0;
    for (int i = 0; i < job_number; i++)
    {
        // if (process_list[i].active) {
        active_processes[active_count++] = process_list[i];
        // }
    }

    // Sort processes lexicographically by command name
    qsort(active_processes, active_count, sizeof(process_info_t), compare_processes);
    char status[1024];
    // Display sorted processes
    for (int i = 0; i < active_count; i++)
    {
        // if(active_processes[i].background==0)
        // continue;
        if (active_processes[i].state == 1)
        {
            snprintf(status, sizeof(status), "Running");
        }
        else if (active_processes[i].state == 0)
        {
            snprintf(status, sizeof(status), "Stopped");
        }
        else if (active_processes[i].state == 2)
        {
            // snprintf(status, sizeof(status), "Terminated");
            continue;
        }

        printf("[%d] : %s - %s\n",
               active_processes[i].pid,
               active_processes[i].command_name,
               status);
    }

    free(active_processes);
    return 0;
}