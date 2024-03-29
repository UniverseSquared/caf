#include "editor.h"
#include "terminal.h"
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
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

    for(size_t i = 0; i < line_display_count; i++) {
        if(strlen(editor.buffer.lines[i + editor.buffer.vscroll]) < editor.buffer.hscroll) {
            printf("\r\n");
            continue;
        }

        char *line = editor.buffer.lines[i + editor.buffer.vscroll] + editor.buffer.hscroll;
        size_t line_length = strlen(line);

        if(line_length < editor.term_width)
            printf("%s\r\n", line);
        else if(line_length > editor.term_width)
            printf("%.*s>\r\n", editor.term_width - 1, line);
        else
            printf("\r\n");
    }

    printf("\x1b[%zu;1H", editor.term_height);

    if(editor.message != NULL)
        printf("%s", editor.message);
    else
        printf("%s %c - %d,%d", editor.buffer.name,
               editor.buffer.modified ? '*' : ' ',
               editor.buffer.cursor_y, editor.buffer.cursor_x);

    size_t cursor_display_x = editor.buffer.cursor_x - editor.buffer.hscroll;
    size_t cursor_display_y = editor.buffer.cursor_y - editor.buffer.vscroll;

    printf("\x1b[%zu;%zuH", cursor_display_y + 1, cursor_display_x + 1);
    fflush(stdout);
}

void editor_insert_char_at_cursor(char c) {
    size_t x = editor.buffer.cursor_x;
    size_t y = editor.buffer.cursor_y;
    size_t line_length = strlen(editor.buffer.lines[y]);
    editor.buffer.lines[y] = realloc(editor.buffer.lines[y], line_length + 2);
    memset(editor.buffer.lines[y] + line_length, 0, 2);

    memmove(editor.buffer.lines[y] + x,
            editor.buffer.lines[y] + x - 1,
            line_length - x + 1);

    editor.buffer.lines[y][x] = c;
    editor.buffer.cursor_x++;
    editor.buffer.modified = 1;

    render_editor_state();
}

void editor_insert_newline_at_cursor(void) {
    size_t x = editor.buffer.cursor_x;
    size_t y = editor.buffer.cursor_y;
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

    move_editor_cursor(0, 1, 1, 1);

    editor.buffer.line_count++;
    editor.buffer.modified = 1;

    render_editor_state();
}

void editor_backspace_at_cursor(void) {
    size_t x = editor.buffer.cursor_x;
    size_t y = editor.buffer.cursor_y;
    size_t line_length = strlen(editor.buffer.lines[y]);

    if(x == 0 && y == 0)
        return;

    if(x > 0) {
        memmove(editor.buffer.lines[y] + x - 1,
                editor.buffer.lines[y] + x,
                line_length - x + 1);

        editor.buffer.cursor_x--;
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
        editor.buffer.cursor_x = total_line_size - 1;

        size_t relative_x = editor.buffer.cursor_x - total_line_size - 1;

        move_editor_cursor(0, -1, 1, 1);
    }

    editor.buffer.modified = 1;

    render_editor_state();
}

void editor_delete_at_cursor(void) {
    size_t x = editor.buffer.cursor_x;
    size_t y = editor.buffer.cursor_y;
    size_t line_length = strlen(editor.buffer.lines[y]);

    if(x == line_length && y == editor.buffer.line_count - 1)
        return;

    if(x < line_length) {
        memmove(editor.buffer.lines[y] + x,
                editor.buffer.lines[y] + x + 1,
                line_length - x + 1);
    } else if(x < editor.buffer.line_count) {
        size_t next_line_length = strlen(editor.buffer.lines[y + 1]);

        memmove(editor.buffer.lines[y] + line_length,
                editor.buffer.lines[y + 1],
                next_line_length);

        for(size_t i = y + 1; i < editor.buffer.line_count - 1; i++) {
            editor.buffer.lines[i] = editor.buffer.lines[i + 1];
        }

        editor.buffer.line_count--;
    }

    editor.buffer.modified = 1;

    render_editor_state();
}

void editor_save_buffer(void) {
    if(editor.buffer.file == NULL)
        return;

    if(!editor.buffer.modified) {
        editor_show_message("No changes to be saved");
        return;
    }

    ftruncate(fileno(editor.buffer.file), 0);
    fseek(editor.buffer.file, 0, SEEK_SET);

    for(size_t i = 0; i < editor.buffer.line_count; i++) {
        char *line = editor.buffer.lines[i];

        fwrite(line, 1, strlen(line), editor.buffer.file);
        fwrite("\n", 1, 1, editor.buffer.file);
    }

    editor.buffer.modified = 0;

    editor_show_message("Saved buffer");
}

void move_editor_cursor(int x, int y, int render, int relative) {
    size_t new_x = x;
    size_t new_y = y;

    if(relative) {
        new_x += editor.buffer.cursor_x;
        new_y += editor.buffer.cursor_y;
    }

    if(new_x >= 0) {
        editor.buffer.cursor_x = new_x;
    }

    if(new_y >= 0
       && new_y <= editor.buffer.line_count - 1) {
        if(new_y >= editor.term_height + editor.buffer.vscroll - 1) {
            editor.buffer.vscroll++;
        } else if(new_y < editor.buffer.vscroll) {
            editor.buffer.vscroll--;
        }

        editor.buffer.cursor_y = new_y;

        size_t cursor_x_limit = strlen(editor.buffer.lines[new_y]);

        editor.buffer.cursor_x = MIN(editor.buffer.cursor_x, cursor_x_limit);
    }

    if(editor.buffer.cursor_x >= editor.term_width + editor.buffer.hscroll - 1) {
        editor.buffer.hscroll += editor.term_width - 1;
    } else if(editor.buffer.cursor_x < editor.buffer.hscroll) {
        editor.buffer.hscroll -= editor.term_width - 1;
    }

    if(render)
        render_editor_state();
}

void editor_move_backward_paragraph(void) {
    if(editor.buffer.cursor_y == 0)
        return;

    size_t y = editor.buffer.cursor_y - 1;
    while(y > 0 && strlen(editor.buffer.lines[y]) != 0) {
        move_editor_cursor(0, -1, 0, 1);
        y = editor.buffer.cursor_y;
    }

    render_editor_state();
}

void editor_move_forward_paragraph(void) {
    if(editor.buffer.cursor_y == editor.buffer.line_count - 1)
        return;

    size_t y = editor.buffer.cursor_y + 1;
    while(y != editor.buffer.line_count - 1 && strlen(editor.buffer.lines[y]) != 0) {
        move_editor_cursor(0, 1, 0, 1);
        y = editor.buffer.cursor_y;
    }

    render_editor_state();
}

int editor_read_key(size_t *modifiers_out) {
    char c = 0;
    if(read(STDIN_FILENO, &c, 1) == -1) {
        perror("read");
        exit(1);
    }

    if(c == 0x1b) {
        size_t modifiers = 0;
        char buffer[5];
        if(read(STDIN_FILENO, buffer, 5) == -1) {
            perror("read");
            exit(1);
        }

        if(buffer[0] == '[') {
            size_t i = 1;
            if(buffer[1] == '1' && buffer[2] == ';') {
                if(buffer[3] == '2')
                    modifiers |= MODIFIER_SHIFT;
                else if(buffer[3] == '5')
                    modifiers |= MODIFIER_CTRL;

                i += 3;
            }

            if(modifiers_out != NULL)
                *modifiers_out = modifiers;

            if(buffer[i] == 'A')
                return KEY_ARROW_UP;
            else if(buffer[i] == 'B')
                return KEY_ARROW_DOWN;
            else if(buffer[i] == 'C')
                return KEY_ARROW_RIGHT;
            else if(buffer[i] == 'D')
                return KEY_ARROW_LEFT;
            else if(buffer[i] == '3' && buffer[2] == '~')
                return KEY_DELETE;
            else if(buffer[i] == 'H')
                return KEY_HOME;
            else if(buffer[i] == 'F')
                return KEY_END;
        }

        return 0;
    }

    return c;
}

void editor_handle_keypress() {
    size_t modifiers;
    int key = editor_read_key(&modifiers);

    if(key == 0)
        return;

    if(editor.message != NULL) {
        editor.message = NULL;
        render_editor_state();
    }

    switch(key) {
    case CTRL('q'):
        editor_quit();
        break;

    case CTRL('z'):
        editor_suspend();
        break;

    case CTRL('p'):
    case KEY_ARROW_UP:
        if(modifiers & MODIFIER_CTRL)
            editor_move_backward_paragraph();
        else
            move_editor_cursor(0, -1, 1, 1);
        break;

    case CTRL('n'):
    case KEY_ARROW_DOWN:
        if(modifiers & MODIFIER_CTRL)
            editor_move_forward_paragraph();
        else
            move_editor_cursor(0, 1, 1, 1);
        break;

    case CTRL('b'):
    case KEY_ARROW_LEFT:
        move_editor_cursor(-1, 0, 1, 1);
        break;

    case CTRL('f'):
    case KEY_ARROW_RIGHT:
        move_editor_cursor(1, 0, 1, 1);
        break;

    case KEY_END:
    case CTRL('e'): {
        size_t cursor_x_limit = strlen(editor.buffer.lines[editor.buffer.cursor_y]);
        move_editor_cursor(cursor_x_limit, editor.buffer.cursor_y, 1, 0);
    } break;

    case KEY_HOME:
    case CTRL('a'):
        move_editor_cursor(0, editor.buffer.cursor_y, 1, 0);
        break;

    case CTRL('s'):
        editor_save_buffer();
        break;

    case KEY_BACKSPACE:
        editor_backspace_at_cursor();
        break;

    case KEY_DELETE:
        editor_delete_at_cursor();
        break;

    case KEY_RETURN:
        editor_insert_newline_at_cursor();
        break;

    default:
        editor_insert_char_at_cursor(key);
    }
}

void editor_load_from_file(char *file_path) {
    FILE *file = fopen(file_path, "r+");
    if(!file) {
        printf("Couldn't open file: %s\n", strerror(errno));
        exit(1);
    }

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
    editor.buffer.hscroll = 0;
    editor.buffer.vscroll = 0;
    editor.buffer.file = file;
    editor.buffer.name = basename(file_path);
    editor.buffer.modified = 0;
}

void editor_show_message(char *message) {
    editor.message = message;
    render_editor_state();
}

int editor_prompt_ync(const char *prompt) {
    const char *prompt_suffix = " (y, n or c) ";
    size_t total_buffer_size = strlen(prompt) + strlen(prompt_suffix) + 1;

    char *buffer = malloc(total_buffer_size);
    snprintf(buffer, total_buffer_size, "%s%s", prompt, prompt_suffix);

    editor_show_message(buffer);

    printf("\x1b[%d;%dH", editor.term_height, total_buffer_size);
    fflush(stdout);

    ssize_t read_count;
    char c;
    while(1) {
        c = editor_read_key(NULL);

        if(read_count == -1) {
            perror("read");
            exit(1);
        }

        if(c == 'n' || c == 'N')
            return 0;
        else if(c == 'y' || c == 'Y')
            return 1;
        else if(c == 'c' || c == 'C')
            return 2;
    }

    editor_show_message(NULL);
}

void editor_quit(void) {
    if(editor.buffer.modified) {
        int response = editor_prompt_ync("Buffer is modified; save?");
        if(response == RESPONSE_YES)
            editor_save_buffer();
        else if(response == RESPONSE_CANCEL)
            return;
    }

    editor.running = 0;
}

void editor_window_size_changed(int signum) {
    struct winsize size;
    ioctl(0, TIOCGWINSZ, &size);

    editor.term_width = size.ws_col;
    editor.term_height = size.ws_row;

    render_editor_state();
}

void editor_suspend(void) {
    /* Restore the original terminal configuration and clear the screen. */
    restore_terminal_configuration();
    printf("\x1b[2J");
    fflush(stdout);

    /* To suspend the editor, we can just SIGSTOP ourselves. */
    kill(0, SIGSTOP);
}

void editor_continue(void) {
    /* After returning from suspension, we restore our terminal configuration
     * and re-render the editor state. */
    configure_terminal();
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

    /* Handle suspension (with C-z or SIGTSTP) properly. */
    signal(SIGTSTP, editor_suspend);
    signal(SIGCONT, editor_continue);

    editor.buffer.cursor_x = 0;
    editor.buffer.cursor_y = 0;

    editor.running = 1;
}

void cleanup_editor(void) {
    if(editor.buffer.file != NULL) {
        fclose(editor.buffer.file);
    }
}
