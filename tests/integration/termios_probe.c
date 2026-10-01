/* Observe restoration before Darwin revokes a session leader's terminal on exit.
 * This test-only destructor has no production hook and changes no terminal state. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

__attribute__((destructor)) static void capture_terminal(void)
{
    const char *path = getenv("YVEX_TEST_TERMIOS_RECEIPT");
    struct termios value;
    FILE *output;
    int fd, index;
    if (!path || tcgetattr(STDIN_FILENO, &value) != 0) return;
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return;
    output = fdopen(fd, "w");
    if (!output) { close(fd); return; }
    fprintf(output, "[%llu,%llu,%llu,%llu,%llu,%llu,[",
        (unsigned long long)value.c_iflag, (unsigned long long)value.c_oflag,
        (unsigned long long)value.c_cflag, (unsigned long long)value.c_lflag,
        (unsigned long long)cfgetispeed(&value), (unsigned long long)cfgetospeed(&value));
    for (index = 0; index < NCCS; ++index)
        fprintf(output, "%s%u", index ? "," : "", value.c_cc[index]);
    fprintf(output, "]]\n");
    fclose(output);
}
