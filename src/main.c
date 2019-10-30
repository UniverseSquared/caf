#include "editor.h"
#include "terminal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    init_editor();

    FILE *file = fopen("src/terminal.c", "r");
    editor_load_from_file(file);
    fclose(file);

    render_editor_state();

    char c;
    while(editor.running) {
        c = 0;

        if(read(STDIN_FILENO, &c, 1) == -1) {
            perror("read");
            exit(1);
        }

        if(c != 0)
            editor_handle_keypress(c);
    }

    return 0;
}
