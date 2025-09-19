#include <stdint.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h> // read

#define MAX_PATH_BYTES 4096
#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + MAX_PATH_BYTES)
#define INOTIFY_BUFFER_SIZE (16 * INOTIFY_EVENT_SIZE)
#define MAX_WATCH_COUNT 10

void handle(const char *file_name);

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

    uint8_t inotify_buffer[INOTIFY_BUFFER_SIZE];
    while (1) {
        ssize_t read_bytes = read(inotify_fd, inotify_buffer, INOTIFY_BUFFER_SIZE);
        if (read_bytes <= 0) {
            fprintf(stderr, "inotify read bytes %ld\n", read_bytes);
            return 1;
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
                    handle(file_name);

                    // need to re-add file watches for some reason.
                    int wd = inotify_add_watch(inotify_fd, file_name, IN_MODIFY);
                    if (wd == -1) {
                        fprintf(stderr, "could not re-watch %s\n", file_name);
                        if (--watch_count <= 0) {
                            fprintf(stderr, "ran out of files to watch\n");
                            return 1;
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

    return 0;
}

void handle(const char *file_name) {
    // TODO
}
