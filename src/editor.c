#include "editor.h"
#include "terminal.h"
#include <ctype.h>
#include <signal.h>
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
                                    editor.term_height - 1);

    for(size_t i = 0; i < line_display_count; i++)
        printf("%s\r\n", editor.buffer.lines[i + editor.buffer.scroll]);

    printf("\x1b[%zu;1H", editor.term_height);

    if(editor.message != NULL)
        printf("%s", editor.message);
    else
        printf("caf version %s", CAF_VERSION);

    size_t cursor_display_y = editor.cursor_y - editor.buffer.scroll;

    printf("\x1b[%zu;%zuH", cursor_display_y + 1, editor.cursor_x + 1);
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

void editor_insert_newline_at_cursor(void) {
    size_t x = editor.cursor_x;
    size_t y = editor.cursor_y;
    editor.buffer.lines = realloc(editor.buffer.lines,
                                  sizeof(char*) * editor.buffer.line_count + 1);

    for(size_t i = editor.buffer.line_count; i > y; i--) {
        editor.buffer.lines[i] = editor.buffer.lines[i - 1];
    }

    size_t remaining_line_length = strlen(editor.buffer.lines[y]) - x;

    editor.buffer.lines[y + 1] = malloc(remaining_line_length + 1);
    editor.buffer.lines[y + 1][remaining_line_length] = 0;

    memmove(editor.buffer.lines[y + 1],
            editor.buffer.lines[y] + x,
            remaining_line_length);

    memset(editor.buffer.lines[y] + x, 0, remaining_line_length);



    move_editor_cursor(0, 1);

    editor.buffer.line_count++;

    render_editor_state();
}

void editor_backspace_at_cursor(void) {
    size_t x = editor.cursor_x;
    size_t y = editor.cursor_y;
    size_t line_length = strlen(editor.buffer.lines[y]);

    if(x == 0 && y == 0)
        return;

    if(x > 0) {
        memmove(editor.buffer.lines[y] + x - 1,
                editor.buffer.lines[y] + x,
                line_length - x + 1);

        editor.cursor_x--;
    } else {
        size_t previous_line_length = strlen(editor.buffer.lines[y - 1]);
        size_t total_line_size = previous_line_length + line_length + 1;
        editor.buffer.lines[y - 1] = realloc(editor.buffer.lines[y - 1],
                                             total_line_size);

        memmove(editor.buffer.lines[y - 1] + previous_line_length,
                editor.buffer.lines[y],
                line_length);

        for(size_t i = y + 1; i < editor.buffer.line_count; i++) {
            editor.buffer.lines[i - 1] = editor.buffer.lines[i];
        }

        editor.buffer.line_count--;
        editor.cursor_x = total_line_size - 1;

        size_t relative_x = editor.cursor_x - total_line_size - 1;

        move_editor_cursor(0, -1);
    }

    render_editor_state();
}

void editor_save_buffer(void) {
    if(editor.buffer.file == NULL)
        return;

    ftruncate(fileno(editor.buffer.file), 0);
    fseek(editor.buffer.file, 0, SEEK_SET);

    for(size_t i = 0; i < editor.buffer.line_count; i++) {
        char *line = editor.buffer.lines[i];

        fwrite(line, 1, strlen(line), editor.buffer.file);
        fwrite("\n", 1, 1, editor.buffer.file);
    }

    editor_show_message("saved file");
}

void set_editor_cursor_position(size_t x, size_t y) {
    editor.cursor_x = x;
    editor.cursor_y = y;

    render_editor_state();
}

void move_editor_cursor(int x, int y) {
    size_t new_x = editor.cursor_x + x;
    size_t new_y = editor.cursor_y + y;

    if(new_x >= 0 && new_x < editor.term_width + 1)
        editor.cursor_x += x;

    if(new_y >= 0
       && new_y <= editor.buffer.line_count - 1) {
        if(new_y >= editor.term_height + editor.buffer.scroll - 1) {
            editor.buffer.scroll++;
        } else if(new_y < editor.buffer.scroll) {
            editor.buffer.scroll--;
        }

        editor.cursor_y = new_y;

        size_t cursor_x_limit = strlen(editor.buffer.lines[new_y]);

        editor.cursor_x = MIN(editor.cursor_x, cursor_x_limit);
    }

    render_editor_state();
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

    if(editor.message != NULL) {
        editor.message = NULL;
        render_editor_state();
    }

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

    case CTRL('s'):
        editor_save_buffer();
        break;

    case KEY_BACKSPACE:
        editor_backspace_at_cursor();
        break;

    case KEY_RETURN:
        editor_insert_newline_at_cursor();
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
    editor.buffer.scroll = 0;
    editor.buffer.file = file;
}

void editor_show_message(char *message) {
    editor.message = message;
    render_editor_state();
}

void editor_window_size_changed(int signum) {
    struct winsize size;
    ioctl(0, TIOCGWINSZ, &size);

    editor.term_width = size.ws_col;
    editor.term_height = size.ws_row;

    render_editor_state();
}

void init_editor(void) {
    configure_terminal();

    /* Set the editor size to the size of the terminal. */
    struct winsize size;
    ioctl(0, TIOCGWINSZ, &size);

    editor.term_width = size.ws_col;
    editor.term_height = size.ws_row;

    /* Register a callback for when the window size changes. */
    signal(SIGWINCH, editor_window_size_changed);

    editor.cursor_x = 0;
    editor.cursor_y = 0;

    editor.running = 1;
}

void cleanup_editor(void) {
    if(editor.buffer.file != NULL) {
        editor_save_buffer();
        fclose(editor.buffer.file);
    }
}
