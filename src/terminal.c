#include "editor.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

void restore_terminal_configuration(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.original_termios);

    printf("\x1b[2J\x1b[1;1H");
    fflush(stdout);
}

void configure_terminal(void) {
    if(tcgetattr(STDIN_FILENO, &editor.original_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }

    atexit(restore_terminal_configuration);

    struct termios termios = editor.original_termios;

    termios.c_iflag &= ~(IXON | ICRNL | ISTRIP | INPCK | BRKINT);
    termios.c_oflag &= ~(OPOST);
    termios.c_cflag |= CS8;
    termios.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    /* Make `read` not wait indefinitely for input. */
    termios.c_cc[VMIN] = 0;
    termios.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios) == -1) {
        perror("tcsetattr");
        exit(1);
    }
}
