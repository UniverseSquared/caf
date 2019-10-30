#include "editor.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MIN(x, y) ((x) > (y) ? (y) : (x))

editor_state_t editor = {};

void render_editor_state(void) {
    printf("\x1b[2J\x1b[0;0H");

    size_t line_display_count = MIN(editor.buffer.line_count,
                                    editor.term_height);

    for(size_t i = 0; i < line_display_count; i++)
        printf("%s\r\n", editor.buffer.lines[i]);

    if(editor.buffer.line_count < editor.term_height) {
        size_t remaining = editor.term_height - editor.buffer.line_count - 1;
        for(size_t i = 0; i < remaining; i++)
            printf("\r\n");
    }

    printf("caf version %s", CAF_VERSION);

    printf("\x1b[%zu;%zuH", editor.cursor_y, editor.cursor_x);
    fflush(stdout);
}

void move_editor_cursor(int x, int y) {
    size_t new_x = editor.cursor_x + x;
    size_t new_y = editor.cursor_y + y;

    if(new_x > 0 && new_x < editor.term_width + 1)
        editor.cursor_x += x;

    if(new_y > 0 && new_y < editor.term_height)
        editor.cursor_y += y;

    size_t cursor_x_limit = strlen(editor.buffer.lines[new_y]) + 1;

    editor.cursor_x = MIN(editor.cursor_x, cursor_x_limit);

    printf("\x1b[%zu;%zuH", editor.cursor_y, editor.cursor_x);
    fflush(stdout);
}

void editor_handle_keypress(char c) {
    if(c == CTRL('q')) {
        editor.running = 0;
    } else if(c == 0x1b) {
        char buffer[2];
        if(read(STDIN_FILENO, buffer, 2) == -1) {
            perror("read");
            exit(1);
        }

        if(buffer[0] == '[') {
            if(buffer[1] == 'A')
                move_editor_cursor(0, -1);
            else if(buffer[1] == 'B')
                move_editor_cursor(0, 1);
            else if(buffer[1] == 'C')
                move_editor_cursor(1, 0);
            else if(buffer[1] == 'D')
                move_editor_cursor(-1, 0);
        }
    }
}

void editor_load_from_file(FILE *file) {
    char *line = NULL;
    size_t size, i = 0;
    ssize_t length;

    editor.buffer.lines = malloc(sizeof(char*));

    while((length = getline(&line, &size, file)) != -1) {
        length--;
        while(line[length] == '\n' || line[length] == '\r')
            line[length--] = 0;

        editor.buffer.lines = realloc(editor.buffer.lines, sizeof(char*) * (i + 1));
        editor.buffer.lines[i] = strdup(line);
        i++;
    }

    editor.buffer.line_count = i;
}

void init_editor(void) {
    configure_terminal();

    /* Set the editor size to the size of the terminal. */
    struct winsize size;
    ioctl(0, TIOCGWINSZ, &size);

    editor.term_width = size.ws_col;
    editor.term_height = size.ws_row;

    editor.cursor_x = 1;
    editor.cursor_y = 1;

    editor.running = 1;
}
