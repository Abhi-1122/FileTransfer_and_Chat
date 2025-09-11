#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "file.h"

// Forward declarations
struct atomic;
struct cmd_group;

typedef struct atomic
{
    char * command;
    char * args;
    int redirection; // 0: none, 1: input, 2: output, 3: append, 4: inp&out, 5: inp&append
    char * input_file;
    char * output_file;
    int inp_fail;
    int out_fail;
    struct atomic* next;
} atomic;

typedef struct cmd_group
{
    struct atomic *atom;
    int background;
    int and;
    int semi_col;
    struct cmd_group *next;
} cmd_group;


cmd_group* parse_input(const char *input, char ***parsed);
void free_cmd_groups(cmd_group *head);

#endif
