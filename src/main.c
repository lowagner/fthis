#include <stdint.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h> // read

#define MAX_PATH_BYTES 4096
#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + MAX_PATH_BYTES)
#define INOTIFY_BUFFER_SIZE (4 * INOTIFY_EVENT_SIZE)
#define QUIT_MAIN(r) {result = r; goto quit_main;}

int main(int nargs, const char **vargs) {
    if (nargs != 2) {
        fprintf(stderr, "%s path/to/file.txt # exactly 1 file, please!\n", vargs[0]);
        return 1;
    }
    const char *file_path = vargs[1];

    int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        fprintf(stderr, "could not initialize inotify\n");
        return 1;
    }
    
    int file_wd = inotify_add_watch(inotify_fd, file_path, IN_MODIFY);
    if (file_wd == -1) {
        fprintf(stderr, "could not watch %s\n", file_path);
        return 1;
    }
    fprintf(stderr, "watching %s (wd = %d)\n", file_path, file_wd);

    uint8_t inotify_buffer[INOTIFY_BUFFER_SIZE];
    int result = 0;
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
                QUIT_MAIN(1);
            } else if (event->wd == file_wd) {
                fprintf(stderr, "found event associated with file %s (wd = %d)\n", file_path, file_wd);
                QUIT_MAIN(0);
            } else {
                fprintf(stderr, "got unexpected watch descriptor (wd = %d)\n", event->wd);
            }
            offset += sizeof(struct inotify_event) + event->len;
        }
    }

quit_main:
    return result;
}
