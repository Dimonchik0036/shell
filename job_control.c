/*
 * Copyright © 2018 Dimonchik0036. All rights reserved.
 */


#include "job_control.h"
#include "terminal.h"

#include <wait.h>
#include <signal.h>


JobController *job_controller_create() {
    JobController *controller = malloc(sizeof(JobController));
    check_memory(controller);

    job_controller_init(controller);
    return controller;
}

void job_controller_free(JobController *controller) {
    size_t index;
    for (index = 0; index < controller->number_of_jobs; ++index) {
        job_free(controller->jobs[index]);
    }

    free(controller);
}

void job_controller_init(JobController *controller) {
    memset(controller->jobs, 0, sizeof(Job *) * JOB_LIMIT);
    controller->current_max_jid = 1;
    controller->number_of_jobs = 0;
}

jid_t job_controller_add_job(JobController *controller,
                             pid_t pid,
                             Command const *command,
                             char status) {
    return job_controller_add_conveyor(controller, pid, command, status, 1);
}

jid_t job_controller_add_conveyor(JobController *controller,
                                  pid_t pid,
                                  Command const *command,
                                  char status,
                                  size_t job_count) {
    if (controller->number_of_jobs >= JOB_LIMIT - 1) {
        fprintf(stderr, "shell: number of jobs (%d) exceeded\n", JOB_LIMIT);
        return BAD_RESULT;
    }

    Command *copy_of_command = command_copy_for_job(command);
    Job *job = job_create_conveyor(controller->current_max_jid++, pid, copy_of_command,
                          status, job_count);
    controller->jobs[controller->number_of_jobs++] = job;

    fprintf(stderr, "\n[%d] %d\n", job->jid, (int) job->pid);
    return job->jid;
}

int job_controller_remove_job_by_index(JobController *controller,
                                       size_t index) {
    Job *current_job = controller->jobs[index];

    size_t last_index = (size_t) (controller->number_of_jobs - 1);
    memmove(controller->jobs + index, controller->jobs + index + 1,
            (last_index - index) * sizeof(Job *));
    --controller->number_of_jobs;
    controller->jobs[controller->number_of_jobs] = NULL;

    if (!controller->number_of_jobs) {
        controller->current_max_jid = 1;
    }

    job_free(current_job);
    return EXIT_SUCCESS;
}

int job_controller_release(JobController *controller) {
    size_t index;
    for (index = 0; index < controller->number_of_jobs; ++index) {
        Job *current_job = controller->jobs[index];
        killpg(current_job->pid, SIGHUP);
    }

    if (controller->number_of_jobs) {
        printf("There are stopped jobs.\n");
    }

    return EXIT_SUCCESS;
}

void job_controller_print_all_jobs(JobController *controller) {
    size_t index;
    for (index = 0; index < controller->number_of_jobs; ++index) {
        Job *current_job = controller->jobs[index];
        job_print(current_job, stdout, "");
    }
}

void job_controller_print_current_status(JobController *controller) {
    size_t index;
    for (index = 0; index < controller->number_of_jobs; ++index) {
        Job *current_job = controller->jobs[index];

        int status;
        pid_t answer = waitpid(-current_job->pid, &status, WNOHANG | WUNTRACED);
        while (answer != 0 && answer != BAD_RESULT) {
            --current_job->count;
            answer = waitpid(-current_job->pid, &status, WNOHANG | WUNTRACED);
        }

        if (answer == 0) {
            continue;
        }

        job_set_status(current_job, status);
        job_print(current_job, stdout, "\n");
        if (WIFEXITED(status)) {
            job_controller_remove_job_by_index(controller, index);
            --index;
        }
    }
}

size_t job_controller_search_job_by_jid(JobController *controller, jid_t jid) {
    size_t index;
    for (index = 0; index < controller->number_of_jobs; ++index) {
        Job *current_job = controller->jobs[index];
        if (current_job->jid == jid) {
            return index;
        }
    }

    return (size_t) controller->number_of_jobs + 1;
}
