/*
 * Copyright © 2018 Dimonchik0036. All rights reserved.
 */


#include "builtin.h"
#include "execute.h"
#include "terminal.h"

#include <signal.h>


#define EQUALS 0


static int builtin_cd(Command *command);

static int builtin_jobs(JobController *controller, Command *command);

static int builtin_fg(JobController *controller, Command *command);

static int builtin_bg(JobController *controller, Command *command);

static int builtin_exit(JobController *controller);

static int builtin_jkill(JobController *controller, Command *command);

static size_t job_get_index(JobController *controller, char *str);


int builtin_exec(JobController *controller, Command *command) {
    char *command_name = command_get_name(command);

    if (strcmp(command_name, "cd") == EQUALS) {
        return builtin_cd(command);
    } else if (strcmp(command_name, "jobs") == EQUALS) {
        return builtin_jobs(controller, command);
    } else if (strcmp(command_name, "fg") == EQUALS) {
        return builtin_fg(controller, command);
    } else if (strcmp(command_name, "bg") == EQUALS) {
        return builtin_bg(controller, command);
    } else if (strcmp(command_name, "jkill") == EQUALS) {
        return builtin_jkill(controller, command);
    } else if (strcmp(command_name, "exit") == EQUALS) {
        return builtin_exit(controller);
    }

    return CONTINUE;
}

static int builtin_cd(Command *command) {
    if (command->arguments[1] && command->arguments[2]) {
        fprintf(stderr, "cd: too many arguments");
        return CRASH;
    } else {
        int exit_code = chdir(command->arguments[1]);
        if (exit_code == BAD_RESULT) {
            perror("cd");
            return CRASH;
        }
    }

    return STOP;
}

static int builtin_jobs(JobController *controller, Command *command) {
    if (command->arguments[1] != NULL) {
        fprintf(stderr, "shell: jobs: too many arguments\n");
        return CRASH;
    }

    job_controller_print_all_jobs(controller);
    return STOP;
}

static int builtin_exit(JobController *controller) {
    job_controller_release(controller);
    return EXIT;
}

static int builtin_fg(JobController *controller, Command *command) {
    if (!controller->number_of_jobs) {
        fprintf(stderr, "shell: fg: current: no such job\n");
        return CRASH;
    }

    if (command->arguments[1] != NULL && command->arguments[2] != NULL) {
        fprintf(stderr, "shell: fg: too many arguments\n");
        return CRASH;
    }

    size_t job_index = job_get_index(controller, command->arguments[1]);
    if (job_index >= controller->number_of_jobs) {
        fprintf(stderr, "shell: fg:  %s: no such job\n", command->arguments[1]);
        return CRASH;
    }

    Job *job = controller->jobs[job_index];
    int exit_code = terminal_set_stdin(job->pid);
    if (exit_code == BAD_RESULT) {
        return CRASH;
    }

    job_killpg(job, SIGCONT);
    job_wait(job);

    pid_t pgrp = getpgrp();
    exit_code = terminal_set_stdin(pgrp);
    if (exit_code == BAD_RESULT) {
        return CRASH;
    }

    if (job->status & JOB_DONE) {
        printf("%s\n", command_get_name(job->command));
        job_controller_remove_job_by_index(controller, job_index);
    }

    return STOP;
}

static int builtin_bg(JobController *controller, Command *command) {
    if (!controller->number_of_jobs) {
        fprintf(stderr, "shell: bg: current: no such job\n");
        return CRASH;
    }

    if (command->arguments[1] != NULL && command->arguments[2] != NULL) {
        fprintf(stderr, "shell: bg: too many arguments\n");
        return CRASH;
    }

    size_t job_index = job_get_index(controller, command->arguments[1]);
    if (job_index >= controller->number_of_jobs) {
        fprintf(stderr, "shell: bg:  %s: no such job\n", command->arguments[1]);
        return CRASH;
    }

    Job *job = controller->jobs[job_index];
    job_killpg(job, SIGCONT);
    job->status = JOB_RUNNING;
    job_print(job, stdout, "");
    return STOP;
}

static int builtin_jkill(JobController *controller, Command *command) {
    if (!controller->number_of_jobs) {
        fprintf(stderr, "shell: jkill: current: no such job\n");
        return CRASH;
    }

    if (command->arguments[1] != NULL && command->arguments[2] != NULL) {
        fprintf(stderr, "shell: jkill: too many arguments\n");
        return CRASH;
    }

    size_t job_index = job_get_index(controller, command->arguments[1]);
    if (job_index >= controller->number_of_jobs) {
        fprintf(stderr, "shell: jkill:  %s: no such job\n",
                command->arguments[1]);
        return CRASH;
    }

    Job *job = controller->jobs[job_index];
    job_killpg(job, SIGKILL);
    printf("Done\n");
    job_controller_remove_job_by_index(controller, job_index);
    return STOP;
}

static size_t job_get_index(JobController *controller, char *str) {
    size_t job_index = (size_t) (controller->number_of_jobs - 1);
    if (str) {
        size_t curr_index = 0;
        for (curr_index = 0; curr_index < strlen(str); ++curr_index) {
            if (!isdigit(str[curr_index])) {
                return (size_t) controller->number_of_jobs;
            }
        }

        int jid = atoi(str);
        if (jid) {
            job_index = job_controller_search_job_by_jid(controller, jid);
        }
    }

    return job_index;
}
