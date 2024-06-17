#include <inc/lib.h>

void
umain(int argc, char **argv) {
    int fd, r, i;
    char filename[] = "test_000000";
    char buf[16 * 1024] = {};
    for (i = 0; i < 16 * 1024; i++)
        buf[i] = 'A';
    for (i = 0; i < 10000000; i++) {
        if (i % 1000 == 0)
            cprintf("Written %d files\n", i);
        for (int k = 1; k < 7; k++) {
            if (filename[sizeof(filename) - k - 1] == '9') {
                filename[sizeof(filename) - k - 1] = '0';
                continue;
            }
            filename[sizeof(filename) - k - 1] += 1;
            break;
        }
        if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC)) < 0) {
            cprintf("open %s for write: %i", filename, fd);
            exit();
        }
        if ((r = write(fd, buf, sizeof(buf))) < 0)
            panic("write: %ld", (long)r);
        close(fd);
    }
}