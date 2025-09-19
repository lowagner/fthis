#include <signal.h> // SIGKILL
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // system
#include <string.h>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h> // read

#define MAX_PATH_BYTES 4096
#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + MAX_PATH_BYTES)
#define INOTIFY_BUFFER_SIZE (16 * INOTIFY_EVENT_SIZE)
#define MAX_WATCH_COUNT 10
#define SHELL "bash"
#define QUIT_MAIN(r) {result = r; goto quit_main;}
#define READ_PIPE 0
#define WRITE_PIPE 1

void handle(const char *file_name, int child_in, int child_out);
int create_child_shell(int *child_pid, int *child_in, int *child_out);

int main(int nargs, const char **vargs) {
    if (nargs < 2) {
        printf("%s [file_path]\n", vargs[0]);
        return 1;
    }
    if (nargs >= MAX_WATCH_COUNT) {
        printf("%s [file_path] # with fewer files, please\n", vargs[0]);
        return 1;
    }

    int descriptors[MAX_WATCH_COUNT] = {0};

    int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        fprintf(stderr, "could not initialize inotify\n");
        return 1;
    }

    int watch_count = 0;
    for (int c = 1; c < nargs; ++c) {
        int wd = inotify_add_watch(inotify_fd, vargs[c], IN_MODIFY);
        if (wd == -1) {
            fprintf(stderr, "could not watch %s\n", vargs[c]);
        } else {
            printf("watching %s (wd = %d)\n", vargs[c], wd);
            watch_count += 1;
            descriptors[c] = wd;
        }
    }
    if (watch_count == 0) {
        fprintf(stderr, "no files could be watched, exiting\n");
        return 1;
    }

    int child_pid, child_in, child_out;
    if (!create_child_shell(&child_pid, &child_in, &child_out)) {
        fprintf(stderr, "couldn't open child shell\n");
        return 1;
    }

    int result = 0;
    uint8_t inotify_buffer[INOTIFY_BUFFER_SIZE];
    while (1) {
        ssize_t read_bytes = read(inotify_fd, inotify_buffer, INOTIFY_BUFFER_SIZE);
        if (read_bytes <= 0) {
            fprintf(stderr, "inotify read bytes %ld\n", read_bytes);
            QUIT_MAIN(1);
        }

        ssize_t offset = 0;
        while (offset < read_bytes) {
            struct inotify_event *event = (struct inotify_event *)(inotify_buffer + offset);
            if (event->len > 0) {
                fprintf(
                    stderr,
                    "ignoring inotify event for file %s from directory wd = %d\n",
                    event->name, event->wd
                );
            } else {
                int name_offset = 0;
                for (int c = 1; c < MAX_WATCH_COUNT; ++c) {
                    if (event->wd == descriptors[c]) {
                        name_offset = c;
                        break;
                    }
                }
                if (name_offset == 0) {
                    fprintf(
                        stderr,
                        "unknown file name from wd = %d\n",
                        event->wd
                    );
                } else {
                    const char *file_name = vargs[name_offset];
                    printf("handling file %s update (wd = %d)\n", file_name, event->wd);
                    handle(file_name, child_in, child_out);

                    // need to re-add file watches for some reason.
                    int wd = inotify_add_watch(inotify_fd, file_name, IN_MODIFY);
                    if (wd == -1) {
                        fprintf(stderr, "could not re-watch %s\n", file_name);
                        if (--watch_count <= 0) {
                            fprintf(stderr, "ran out of files to watch\n");
                            QUIT_MAIN(1);
                        }
                    } else {
                        printf("rewatching %s (wd = %d)\n", file_name, wd);
                        descriptors[name_offset] = wd;
                    }
                    // NOTE: cleanup doesn't seem necessary, i'm getting errors here:
                    // int error = inotify_rm_watch(inotify_fd, event->wd);
                    // if (error != 0) {
                    //     fprintf(stderr, "had error removing wd %d: %d\n", event->wd, error);
                    // }
                }
            }
            offset += sizeof(struct inotify_event) + event->len;
        }
    }

quit_main:
    kill(child_pid, SIGKILL);
    int status;
    waitpid(child_pid, &status, 0);
    return result;
}

int create_child_shell(int *child_pid, int *child_in, int *child_out) {
    int child_input[2];
    int child_output[2];
    if (pipe(child_input) == -1) goto input_error;
    if (pipe(child_output) == -1) goto output_error;

    int pid = fork();
    if (pid < 0) goto fork_error;
    if (pid == 0) {
        // child
        // use pipe input for child stdin 
        dup2(child_input[READ_PIPE], STDIN_FILENO);
        // redirect child stderr/stdout to pipe output
        dup2(child_output[WRITE_PIPE], STDOUT_FILENO);
        dup2(child_output[WRITE_PIPE], STDERR_FILENO);

        // kill child if parent dies
        prctl(PR_SET_PDEATHSIG, SIGTERM);

        // child doesn't use these (e.g., no writing to input, no reading from output)
        close(child_input[WRITE_PIPE]);
        close(child_output[READ_PIPE]);

        execl("usr/bin/" SHELL, "--init-file", "<(echo \"source ~/." SHELL "rc\")", NULL);
        // shouldn't be exited unless exec failed
        exit(1);
    } else {
        // parent
        *child_pid = pid;
        // parent doesn't use these (e.g., no reading from input, no writing to output)
        close(child_input[READ_PIPE]);
        close(child_output[WRITE_PIPE]);

        *child_in = child_input[WRITE_PIPE];
        *child_out = child_output[READ_PIPE];
        return 1;
    }

fork_error:
    close(child_output[1]);
    close(child_output[0]);
output_error:
    close(child_input[1]);
    close(child_input[0]);
input_error:
    return 0;
}

void handle(const char *file_name, int child_in, int child_out) {
    // TODO
    const char *msg = "echo asdf; echo $b\n";
    write(child_in, msg, strlen(msg));
    char buffer[256];
    read(child_out, buffer, strlen(buffer));
    printf("got message: %s\n", buffer);
}
