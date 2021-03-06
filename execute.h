/*
 * Copyright © 2018 Dimonchik0036. All rights reserved.
 */


#ifndef EXECUTE_H
#define EXECUTE_H


#include "command.h"
#include "job_control.h"


#define EXIT 1
#define CONTINUE 0
#define CRASH (-1)
#define STOP 2

#define BAD_PID (-1)
#define DESCENDANT_PID 0


int execute_command_line(JobController *controller,
                         CommandLine *command_line,
                         ssize_t number_of_commands);


#endif //EXECUTE_H
