#include <stdint.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h> // read

#define MAX_PATH_BYTES 4096
#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + MAX_PATH_BYTES)
#define INOTIFY_BUFFER_SIZE (16 * INOTIFY_EVENT_SIZE)

int main(int nargs, const char **vargs) {
    if (nargs < 2) {
        printf("%s [file_path]\n", vargs[0]);
        return 1;
    }

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
            printf("inotify event wd = %d", event->wd);
            if (event->len > 0) {
                printf(", name = %s", event->name);
            }
            printf("\n");
            offset += sizeof(struct inotify_event) + event->len;
        }
    }

    return 0;
}
