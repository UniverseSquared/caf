#include "editor.h"
#include "terminal.h"
#include <ctype.h>
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

    printf("\x1b[%zu;%zuH", editor.cursor_y + 1, editor.cursor_x + 1);
    fflush(stdout);
}

void editor_insert_char_at_cursor(char c) {
    size_t x = editor.cursor_x;
    size_t y = editor.cursor_y;
    size_t line_length = strlen(editor.buffer.lines[y]);
    editor.buffer.lines[y] = realloc(editor.buffer.lines[y], line_length + 2);
    memset(editor.buffer.lines[y] + line_length, 0, 2);

    memmove(editor.buffer.lines[y] + x,
            editor.buffer.lines[y] + x - 1,
            line_length - x + 1);

    editor.buffer.lines[y][x] = c;
    editor.cursor_x++;
    render_editor_state();
}

void set_editor_cursor_position(size_t x, size_t y) {
    editor.cursor_x = x;
    editor.cursor_y = y;

    printf("\x1b[%zu;%zuH", editor.cursor_y + 1, editor.cursor_x + 1);
    fflush(stdout);
}

void move_editor_cursor(int x, int y) {
    size_t new_x = editor.cursor_x + x;
    size_t new_y = editor.cursor_y + y;

    if(new_x >= 0 && new_x < editor.term_width + 1)
        editor.cursor_x += x;

    if(new_y >= 0 && new_y < editor.term_height)
        editor.cursor_y += y;

    size_t cursor_x_limit = strlen(editor.buffer.lines[new_y]);

    editor.cursor_x = MIN(editor.cursor_x, cursor_x_limit);

    printf("\x1b[%zu;%zuH", editor.cursor_y + 1, editor.cursor_x + 1);
    fflush(stdout);
}

int editor_read_key(void) {
    char c = 0;
    if(read(STDIN_FILENO, &c, 1) == -1) {
        perror("read");
        exit(1);
    }

    if(c == 0x1b) {
        char buffer[2];
        if(read(STDIN_FILENO, buffer, 2) == -1) {
            perror("read");
            exit(1);
        }

        if(buffer[0] == '[') {
            if(buffer[1] == 'A')
                return KEY_ARROW_UP;
            else if(buffer[1] == 'B')
                return KEY_ARROW_DOWN;
            else if(buffer[1] == 'C')
                return KEY_ARROW_RIGHT;
            else if(buffer[1] == 'D')
                return KEY_ARROW_LEFT;
        } else {
            return 0;
        }
    }

    return c;
}

void editor_handle_keypress() {
    int key = editor_read_key();

    if(key == 0)
        return;

    switch(key) {
    case CTRL('q'):
        editor.running = 0;
        break;

    case CTRL('p'):
    case KEY_ARROW_UP:
        move_editor_cursor(0, -1);
        break;

    case CTRL('n'):
    case KEY_ARROW_DOWN:
        move_editor_cursor(0, 1);
        break;

    case CTRL('b'):
    case KEY_ARROW_LEFT:
        move_editor_cursor(-1, 0);
        break;

    case CTRL('f'):
    case KEY_ARROW_RIGHT:
        move_editor_cursor(1, 0);
        break;

    case CTRL('e'): {
        size_t cursor_x_limit = strlen(editor.buffer.lines[editor.cursor_y]);
        set_editor_cursor_position(cursor_x_limit, editor.cursor_y);
    } break;

    case CTRL('a'):
        set_editor_cursor_position(0, editor.cursor_y);
        break;

    default:
        editor_insert_char_at_cursor(key);
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

    editor.cursor_x = 0;
    editor.cursor_y = 0;

    editor.running = 1;
}
