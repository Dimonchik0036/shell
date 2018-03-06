/*
 * Copyright © 2018 Dimonchik0036. All rights reserved.
 */


#include "execute.h"
#include "builtin.h"
#include "terminal.h"

#include <fcntl.h>
#include <wait.h>
#include <signal.h>


static int execute_parent(JobController *controller,
                          pid_t descendant_pid,
                          CommandLine *command_line,
                          Command *command);

static int execute_descendant(CommandLine *command_line, Command *command);

static int set_infile(char *infile);

static int set_outfile(char *outfile, char addfile);

static int use_dup2(int fd, int fd2, char *error);

static int system_exec(JobController *controller,
                       CommandLine *command_line,
                       Command *command);


int execute_command_line(JobController *controller,
                         CommandLine *command_line,
                         int number_of_commands) {
    int command_index;
    command_line->start_pipeline_index = 0;
    for (command_index = 0; command_index < number_of_commands; ++command_index) {
        Command *current_command = &command_line->commands[command_index];
        command_line->current_pipeline_index = command_index;

        int answer = builtin_exec(controller, current_command);
        switch (answer) {
            case EXIT:
                return answer;
            case CONTINUE:
                break;
            case STOP:
            default:
                continue;
        }

        if (current_command->flag & IN_PIPE) {
            continue;
        }

        answer = system_exec(controller, command_line, current_command);
        switch (answer) {
            case EXIT:
            case CRASH:
                return answer;
            default:
                break;
        }
    }

    return CONTINUE;
}

static int execute_conveer(JobController *controller,
                           CommandLine *command_line,
                           Command *command) {
    pid_t pid = fork();
    if (pid != 0) {
        Command *last_command = command;
        int current_index = command_line->current_pipeline_index;
        for (; command_line->commands[current_index].flag & (IN_PIPE | OUT_PIPE);
               ++current_index) {
            last_command = &command_line->commands[current_index];
        }

        if (last_command->flag & BACKGROUND) {
            job_controller_add_job(controller, pid, last_command, JOB_RUNNING);

            return CONTINUE;
        }

        int status = 0;
        if (waitpid(pid, &status, WUNTRACED) != BAD_RESULT) {
            if (WIFSTOPPED(status)) {
                job_controller_add_job(controller, pid, last_command, JOB_STOPPED);
                return CONTINUE;
            }
        } else {
            perror("Couldn't wait for child process termination");
            return CRASH;
        }

        if (terminal_set_stdin(getpgrp()) == BAD_RESULT) {
            perror("sdsds");
        }
        return CONTINUE;
    }

    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    if (setpgid(0, 0) == BAD_RESULT) {
        perror("Couldn't set process group ID88");
        return CRASH;
    }

//    fprintf(stdout, "97 p%d %d\n", getpid(), getpgrp());
    command_line->start_pipeline_index = command_line->current_pipeline_index;
    int current_index = command_line->current_pipeline_index;
    for (; command_line->commands[current_index].flag & (IN_PIPE | OUT_PIPE);
           ++current_index) {
        if (pipe(command_line->pipe_des) == BAD_RESULT) {
            perror("Couldn't create pipe");
            return CRASH;
        }

        Command *current_command = &command_line->commands[current_index];
        command_line->current_pipeline_index = current_index;
        pid_t pid2 = fork();
        command_line->pids[command_line->current_pipeline_index] = pid2;

        if (pid2 == 0) {
            return execute_descendant(command_line, current_command);
        } else if (pid2 == -1) {
            perror("Couldn't create process");
            return CRASH;
        } else {
            if (current_command->flag & IN_PIPE) {
                close(command_line->prev_out_pipe);
            }

            close(command_line->pipe_des[1]);
            command_line->prev_out_pipe = command_line->pipe_des[0];
            if (current_command->flag & OUT_PIPE) {
                continue;
            }

            int index;
            for (index = command_line->start_pipeline_index;
                 index <= command_line->current_pipeline_index;
                 ++index) {
                pid_t current_pid = command_line->pids[index];

                int status = 0;
                if (waitpid(current_pid, &status, WUNTRACED) != BAD_RESULT) {
                    if (WIFSTOPPED(status)) {
                        fprintf(stdout, "1FSFSF\n");
                    }
                } else {
                    perror("Couldn't wait for child process termination");
                }
            }
        }
    }

    if (terminal_set_stdin(getpgid(getppid())) == BAD_RESULT) {
        perror("ALEEEEEEEEEEEEEERT 169\n");
    }

    exit(EXIT_SUCCESS);
}

static int system_exec(JobController *controller,
                       CommandLine *command_line,
                       Command *command) {
    if (command->flag & OUT_PIPE) {
        return execute_conveer(controller, command_line, command);
    }


    pid_t pid = fork();
    command_line->pids[command_line->current_pipeline_index] = pid;

    switch (pid) {
        case DESCENDANT_PID:
            return execute_descendant(command_line, command);
        case BAD_PID:
            perror("Couldn't create process");
            return CRASH;
        default:
            return execute_parent(controller, pid, command_line, command);
    }
}

static int execute_parent(JobController *controller,
                          pid_t descendant_pid,
                          CommandLine *command_line,
                          Command *command) {
    if (command->flag & IN_PIPE) {
        close(command_line->prev_out_pipe);
    }

    close(command_line->pipe_des[1]);
    command_line->prev_out_pipe = command_line->pipe_des[0];

    if (!(command->flag & IN_PIPE)) {
        command_line->start_pipeline_index = command_line->current_pipeline_index;
    }

    if (command->flag & OUT_PIPE) {
        return CONTINUE;
    }

    if (command->flag & BACKGROUND) {
        job_controller_add_job(controller, descendant_pid, command, JOB_RUNNING);
        return CONTINUE;
    }


    int index;
    for (index = command_line->start_pipeline_index;
         index <= command_line->current_pipeline_index;
         ++index) {
        pid_t current_pid = command_line->pids[index];

        int status = 0;
        if (waitpid(current_pid, &status, WUNTRACED) != BAD_RESULT) {
            if (WIFSTOPPED(status)) {
                job_controller_add_job(controller, current_pid, command, JOB_STOPPED);
                break;
            }
        } else {
            perror("Couldn't wait for child process termination");
        }
    }

    if (terminal_set_stdin(getpgrp()) == BAD_RESULT) {
        return CRASH;
    }

    return CONTINUE;
}

static int execute_descendant(CommandLine *command_line, Command *command) {
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    if (setpgid(0, (command->flag & (IN_PIPE | OUT_PIPE))
                   ? getppid()
                   : 0) == BAD_RESULT) {
        perror("Couldn't set process group ID");
        return CRASH;
    }
//    fprintf(stdout, "242 pp%d p%d g%d\n", getppid(), getpid(), getpgrp());
//    fflush(stdout);
    if (!(command->flag & BACKGROUND) && !(command->flag & IN_PIPE)) {
        if (terminal_set_stdin(getpgrp()) == BAD_RESULT) {
            perror("Couldn't set terminal foreground process group");
            return CRASH;
        }
    }

    if (command->flag & BACKGROUND) {
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
    }

    if ((command->flag & IN_FILE) && command->infile) {
        if (set_infile(command->infile) == CRASH) {
            return CRASH;
        }
    }

    if ((command->flag & OUT_FILE) && command->outfile) {
        if (set_outfile(command->outfile, command->appfile) == CRASH) {
            return CRASH;
        }
    }

    if (command->flag & IN_PIPE) {
        if (use_dup2(command_line->prev_out_pipe, STDIN_FILENO, "Couldn't redirect input")
            == CRASH) {
            return CRASH;
        }
    }

    if (command->flag & OUT_PIPE) {
        if (use_dup2(command_line->pipe_des[1], STDOUT_FILENO, "Couldn't redirect output")
            == CRASH) {
            return CRASH;
        }
    }
    execvp(command->arguments[0], command->arguments);

    perror("Couldn't execute command");
    return CRASH;
}

static int use_dup2(int fd, int fd2, char *error) {
    if (dup2(fd, fd2) == BAD_RESULT) {
        perror(error);
        close(fd);
        return CRASH;
    }
    close(fd);

    return CONTINUE;
}

static int set_infile(char *infile) {
    int input = open(infile, O_RDONLY);
    if (input == BAD_RESULT) {
        perror("Couldn't open input file");
        return CRASH;
    }

    return use_dup2(input, STDIN_FILENO, "Couldn't redirect input");
}

static int set_outfile(char *outfile, char addfile) {
    int flags = O_WRONLY;
    if (addfile) {
        flags |= O_APPEND | O_CREAT;
    } else {
        flags |= O_CREAT | O_TRUNC;
    }

    int output;
    output = open(outfile, flags, (mode_t) 0644);

    if (output == BAD_RESULT) {
        perror("Couldn't open output file");
        return CRASH;
    }

    return use_dup2(output, STDOUT_FILENO, "Couldn't redirect output");;
}
