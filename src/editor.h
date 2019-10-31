#ifndef _EDITOR_H_
#define _EDITOR_H_
#include <stddef.h>
#include <stdio.h>
#include <termios.h>

#define CAF_VERSION "0.0.1"

enum editor_special_key {
    KEY_ARROW_UP = 1000,
    KEY_ARROW_DOWN,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT
};

typedef struct editor_buffer {
    char **lines;
    size_t line_count;
} editor_buffer_t;

typedef struct editor_state {
    struct termios original_termios;
    size_t cursor_x, cursor_y;
    int term_width, term_height;
    int running;
    editor_buffer_t buffer;
} editor_state_t;

extern editor_state_t editor;

void render_editor_state(void);
void editor_insert_char_at_cursor(char c);
void set_editor_cursor_position(size_t x, size_t y);
void move_editor_cursor(int x, int y);
int editor_read_key(void);
void editor_handle_keypress();
void editor_load_from_file(FILE *file);
void editor_window_size_changed(int signum);
void init_editor(void);

#endif
