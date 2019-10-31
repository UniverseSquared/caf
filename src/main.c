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

    while(editor.running) {
        editor_handle_keypress();
    }

    return 0;
}
