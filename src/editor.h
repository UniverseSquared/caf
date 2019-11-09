#ifndef _EDITOR_H_
#define _EDITOR_H_
#include <stddef.h>
#include <stdio.h>
#include <termios.h>

#define BIT(n) (1 << n)

#define CAF_VERSION "0.0.1"

#define RESPONSE_NO     0
#define RESPONSE_YES    1
#define RESPONSE_CANCEL 2

enum editor_special_key {
    KEY_RETURN = 13,
    KEY_BACKSPACE = 127,
    KEY_ARROW_UP = 1000,
    KEY_ARROW_DOWN,
    KEY_ARROW_LEFT,
    KEY_ARROW_RIGHT,
    KEY_DELETE,
    KEY_HOME,
    KEY_END
};

enum editor_key_modifier {
    MODIFIER_CTRL  = BIT(0),
    MODIFIER_SHIFT = BIT(1)
};

typedef struct editor_buffer {
    char **lines;
    size_t line_count;
    size_t cursor_x, cursor_y;
    size_t scroll;
    FILE *file;
    char *name;
    int modified;
} editor_buffer_t;

typedef struct editor_state {
    struct termios original_termios;
    int term_width, term_height;
    int running;
    char *message;
    editor_buffer_t buffer;
} editor_state_t;

extern editor_state_t editor;

void render_editor_state(void);
void editor_insert_char_at_cursor(char c);
void editor_insert_newline_at_cursor(void);
void editor_backspace_at_cursor(void);
void editor_delete_at_cursor(void);
void editor_save_buffer(void);
void set_editor_cursor_position(size_t x, size_t y);
void move_editor_cursor(int x, int y, int render);
void editor_move_backward_paragraph(void);
void editor_move_forward_paragraph(void);
int editor_read_key(size_t *modifiers_out);
void editor_handle_keypress();
void editor_load_from_file(char *file_path);
void editor_show_message(char *message);
int editor_prompt_ync(const char *prompt);
void editor_quit(void);
void editor_window_size_changed(int signum);
void editor_suspend(void);
void editor_continue(void);
void init_editor(void);
void cleanup_editor(void);

#endif
