#include "parser.h"

bool check_valid(char ***parsed, int tokens)
{
    // printf("Checking validity of parsed tokens...\n");
    if ((*parsed)[tokens - 1][0] == '|')
        return false;
    if ((*parsed)[tokens - 1][0] == '<' && (*parsed)[tokens - 1][1] == '\0')
        return false;
    if ((*parsed)[tokens - 1][0] == '>' && (*parsed)[tokens - 1][1] == '\0')
        return false;
    if (strlen((*parsed)[tokens - 1]) == 2)
    {
        if ((*parsed)[tokens - 1][0] == '>' && (*parsed)[tokens - 1][1] == '>')
            return false;
        if ((*parsed)[tokens - 1][0] == '&' && (*parsed)[tokens - 1][1] == '&')
            return false;
    }
    for (int i = 0; i < tokens - 1; i++)
    {
        // int len = strlen((*parsed)[i]);
        if ((*parsed)[i][0] == '|')
        {
            if ((*parsed)[i + 1][0] == '|' || (*parsed)[i + 1][0] == '&' || (*parsed)[i + 1][0] == ';')
                return false;
            if ((*parsed)[i + 1][0] == '&' && (*parsed)[i + 1][1] == '&')
                return false;
        }
        if ((*parsed)[i][0] == '&')
        {
            if ((*parsed)[i + 1][0] == '&' || (*parsed)[i + 1][0] == '|')
                return false;
        }
        if ((*parsed)[i][0] == '&' && (*parsed)[i][1] == '&')
        {
            if ((*parsed)[i + 1][0] == '&' && (*parsed)[i + 1][1] == '&')
                return false;
            if ((*parsed)[i + 1][0] == '|')
                return false;
        }
        if ((*parsed)[i][0] == ';')
        {
            if ((*parsed)[i + 1][0] == '|' || (*parsed)[i + 1][0] == '&')
                return false;
            if ((*parsed)[i + 1][0] == '&' && (*parsed)[i + 1][1] == '&')
                return false;
        }
    }
    return true;
}

cmd_group *build_list(char ***parsed, int tokens)
{
    cmd_group *head = NULL;
    cmd_group *curr_group = NULL;

    atomic *curr_atomic = NULL;
    int inp = 0;

    int i = 0;
    while (i < tokens)
    {
        // If starting a new group
        if (curr_group == NULL)
        {
            curr_group = (cmd_group *)malloc(sizeof(cmd_group));
            curr_group->atom = NULL;
            curr_group->background = 0;
            curr_group->and = 0;
            curr_group->semi_col = 0;
            curr_group->next = NULL;

            // Link to head or previous group
            if (head == NULL)
                head = curr_group;
            else
            {
                cmd_group *tmp = head;
                while (tmp->next)
                    tmp = tmp->next;
                tmp->next = curr_group;
            }
        }

        // If starting a new atomic command
        if (curr_atomic == NULL)
        {
            curr_atomic = (atomic *)malloc(sizeof(atomic));
            curr_atomic->command = NULL;
            curr_atomic->args = NULL;
            curr_atomic->redirection = 0;
            curr_atomic->input_file = NULL;
            curr_atomic->output_file = NULL;
            curr_atomic->next = NULL;
            curr_atomic->inp_fail = 0;
            curr_atomic->out_fail = 0;
            inp = 0;

            // Link into current group's atomic list
            if (curr_group->atom == NULL)
                curr_group->atom = curr_atomic;
            else
            {
                atomic *tmp = curr_group->atom;
                while (tmp->next)
                    tmp = tmp->next;
                tmp->next = curr_atomic;
            }
        }

        char *tok = (*parsed)[i];

        // Handle control operators first
        if (strcmp(tok, "|") == 0)
        {
            // Next atomic command will be created in next loop
            curr_atomic = NULL;
            i++;
            continue;
        }
        else if (strcmp(tok, "&&") == 0)
        {
            curr_group->and = 1;
            curr_atomic = NULL;
            curr_group = NULL; // next token starts a new cmd_group
            i++;
            continue;
        }
        else if (strcmp(tok, "&") == 0)
        {
            curr_group->background = 1;
            curr_atomic = NULL;
            curr_group = NULL;
            i++;
            continue;
        }
        else if (strcmp(tok, ";") == 0)
        {
            curr_group->semi_col = 1;
            curr_atomic = NULL;
            curr_group = NULL;
            i++;
            continue;
        }

        // Handle redirections
        if (tok[0] == '<')
        {
            char* temp = strdup(tok + 1);
            int fail = open_file_for_read(temp);
            if(curr_atomic->inp_fail == 0)
                curr_atomic->input_file = temp;
            if (fail < 0)
                curr_atomic->inp_fail = 1;
            close_file(fail);

            inp = 1;
            i++;
            if (curr_atomic->redirection == 2)
                curr_atomic->redirection = 4;
            else if (curr_atomic->redirection == 3)
                curr_atomic->redirection = 5;
            if (curr_atomic->redirection == 4)
                curr_atomic->redirection = 4;
            else if (curr_atomic->redirection == 5)
                curr_atomic->redirection = 5;
            else
                curr_atomic->redirection = 1;
            continue;
        }
        else if (tok[0] == '>' && tok[1] == '>')
        {
            char* temp = strdup(tok + 2);
            int fail = open_file_for_append(temp);
            if(curr_atomic->out_fail == 0)
                curr_atomic->output_file = temp;
            if (fail < 0)
                curr_atomic->out_fail = 1;
            close_file(fail);


            curr_atomic->redirection = 3;
            i++;
            if (curr_atomic->redirection == 5)
            {
                curr_atomic->redirection = 5;
                continue;
            }
            if (inp == 1)
                curr_atomic->redirection = 5;
            continue;
        }
        else if (tok[0] == '>')
        {
            char* temp = strdup(tok + 1);
            int fail = open_file_for_write(temp);
            if(curr_atomic->out_fail == 0)
                curr_atomic->output_file = temp;
            if (fail < 0)
                curr_atomic->out_fail = 1;
            close_file(fail);


            curr_atomic->redirection = 2;
            i++;
            if (curr_atomic->redirection == 4)
            {
                curr_atomic->redirection = 4;
                continue;
            }
            if (inp == 1)
                curr_atomic->redirection = 4;
            continue;
        }

        // If we haven't set the command yet, this is the command
        if (curr_atomic->command == NULL)
        {
            curr_atomic->command = strdup(tok);
        }
        else
        {
            // Append to args string
            if (curr_atomic->args == NULL)
            {
                curr_atomic->args = strdup(tok);
            }
            else
            {
                // Append with a space
                char *new_args = (char *)malloc(strlen(curr_atomic->args) + strlen(tok) + 2);
                sprintf(new_args, "%s %s", curr_atomic->args, tok);
                free(curr_atomic->args);
                curr_atomic->args = new_args;
            }
        }

        i++;
    }

    return head;
}

cmd_group *parse_input(const char *input, char ***parsed)
{
    int i = 0, j = 0, k = 0;
    int length = strlen(input);

    // Allocate memory for an array of char* (max length words)
    *parsed = (char **)malloc((length + 1) * sizeof(char *));
    if (*parsed == NULL)
    {
        printf("Memory allocation failed while parsing input\n");
        exit(1);
    }
    while (i < length && (input[i] == ' ' || input[i] == '\t' || input[i] == ';' ||
                          input[i] == '\n' || input[i] == '\r'))
    {
        i++;
    }

    // If the string is only whitespace
    if (i >= length)
    {
        return NULL;
    }

    if (input[i] == '|' || input[i] == '>' || input[i] == '<' || input[i] == '&')
        return NULL;

    while (i < length)
    {
        // Skip extra spaces/tabs
        while (i < length && (input[i] == ' ' || input[i] == '\t' ||
                              input[i] == '\n' || input[i] == '\r'))
        {
            i++;
        }
        if (i >= length)
            break;

        // Allocate memory for a token (worst case: remaining string length)
        (*parsed)[k] = (char *)malloc((length - i + 1) * sizeof(char));
        if ((*parsed)[k] == NULL)
        {
            printf("Memory allocation failed while parsing token\n");
            exit(1);
        }
        if (input[i] == ';')
        {
            if (input[i + 1] == ';')
                return NULL;
            (*parsed)[k][0] = ';';
            (*parsed)[k][1] = '\0';
            k++;
            i++;
            continue;
        }
        if (input[i] == '&')
        {
            if (input[i + 1] == '&')
            {
                if (input[i + 2] == '&')
                    return NULL;
                (*parsed)[k][0] = '&';
                (*parsed)[k][1] = '&';
                (*parsed)[k][2] = '\0';
                k++;
                i += 2;
            }
            else
            {
                (*parsed)[k][0] = '&';
                (*parsed)[k][1] = '\0';
                k++;
                i++;
            }
            continue;
        }
        if (input[i] == '|')
        {
            (*parsed)[k][0] = '|';
            (*parsed)[k][1] = '\0';
            k++;
            i++;
            continue;
        }

        j = 0;

        while (i < length && !(input[i] == ' ' || input[i] == '\t' ||
                               input[i] == '\n' || input[i] == '\r' ||
                               input[i] == ';' || input[i] == '&' || input[i] == '|'))
        {
            // printf("Parsing character: %c\n", input[i]);
            if (input[i] == '>')
            {
                if (j != 0)
                    break;
                if (input[i + 1] == '>')
                {
                    i += 2;
                    while (i < length && (input[i] == ' ' || input[i] == '\t' ||
                                          input[i] == '\n' || input[i] == '\r'))
                    {
                        i++;
                    }
                    if (input[i] == ';' || input[i] == '|' || input[i] == '<' || input[i] == '>')
                        return NULL;
                    (*parsed)[k][j++] = '>';
                    (*parsed)[k][j++] = '>';
                }
                else
                {
                    i++;
                    while (i < length && (input[i] == ' ' || input[i] == '\t' ||
                                          input[i] == '\n' || input[i] == '\r'))
                    {
                        i++;
                    }
                    if (input[i] == ';' || input[i] == '|' || input[i] == '<' || input[i] == '>')
                        return NULL;
                    (*parsed)[k][j++] = '>';
                }
            }
            if (input[i] == '<')
            {
                if (j != 0)
                    break;
                i++;
                while (i < length && (input[i] == ' ' || input[i] == '\t' ||
                                      input[i] == '\n' || input[i] == '\r'))
                {
                    i++;
                }
                if (input[i] == ';' || input[i] == '|' || input[i] == '>' || input[i] == '<')
                    return NULL;
                (*parsed)[k][j++] = '<';
            }

            (*parsed)[k][j++] = input[i++];
        }
        (*parsed)[k][j] = '\0'; // Null-terminate token
        k++;
    }
    cmd_group *head = NULL;
    if (check_valid(parsed, k))
    {
        head = build_list(parsed, k);
        for (int m = 0; m < k; m++)
        {
            free((*parsed)[m]);
        }
        free(*parsed);
        *parsed = NULL;
        return head;
    }
    else
    {
        // Free allocated memory for invalid input
        for (int m = 0; m < k; m++)
        {
            free((*parsed)[m]);
        }
        free(*parsed);
        *parsed = NULL;
        return NULL; // Invalid syntax
    }
}

void free_cmd_groups(cmd_group *head)
{
    while (head)
    {
        cmd_group *cg_next = head->next;
        atomic *atom = head->atom;
        while (atom)
        {
            atomic *a_next = atom->next;
            free(atom);
            atom = a_next;
        }
        free(head);
        head = cg_next;
    }
}