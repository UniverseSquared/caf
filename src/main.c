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
    if(argc < 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }

    editor_load_from_file(argv[1]);

    init_editor();

    render_editor_state();

    while(editor.running) {
        editor_handle_keypress();
    }

    cleanup_editor();

    return 0;
}
