#include <stdint.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h> // read

#define MAX_PATH_BYTES 4096
#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + MAX_PATH_BYTES)
#define INOTIFY_BUFFER_SIZE (16 * INOTIFY_EVENT_SIZE)
#define MAX_WATCH_COUNT 10

int main(int nargs, const char **vargs) {
    if (nargs < 2) {
        printf("%s [file_path]\n", vargs[0]);
        return 1;
    }
    if (nargs >= MAX_WATCH_COUNT) {
        printf("%s [file_path] # with fewer files, please\n", vargs[0]);
        return 1;
    }

    int descriptors[MAX_WATCH_COUNT] = {-1};

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
            printf("watching %s with descriptor %d\n", vargs[c], wd);
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
                    // TODO: re-add file after handling it
                }
            }
            offset += sizeof(struct inotify_event) + event->len;
        }
    }

    return 0;
}
