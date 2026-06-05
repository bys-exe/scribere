// header files

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// defined

#define CTRL_KEY(k) ((k) & 0x1f) // ctrl key shorthand
#define SCRIBERE_VERSION "0.0.1"
#define SCRIBERE_TAB_STOP 4
#define SCRIBERE_QUIT_TIMES 3
#define SCRIBERE_LINENUM_WIDTH 5
#define UNDO_STACK_SIZE 1000
enum editorKey
{
    BACKSPACE = 127,
    ARROW_LEFT = 1000, // gave it a huge value which wont conflict ordinary keypresses
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};
enum editorHighlight
{
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

// data

struct editorSyntax
{
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
};
typedef struct erow // editor row
{
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render; // prevents extra spaces in terminal
    unsigned char *hl;
    int hl_open_comment;
} erow;
typedef enum
{ // for undo stack
    ACTION_INSERT,
    ACTION_DELETE
} ActionType;
typedef struct
{
    ActionType type;
    int cy, cx;
    char c;
} UndoAction;
struct editorConfig
{
    int cx, cy;
    int rx; // handling tab spaces
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    int dirty; // to find any unsaved changes (dirty flags)
    char *filename;
    char statusmsg[200];
    time_t statusmsg_time; // cooldown before status msg disappears
    int msg_is_permanent;  // for a permanent message in status bar
    struct editorSyntax *syntax;
    struct termios orig_termios;
    UndoAction undo_stack[UNDO_STACK_SIZE];
    int undo_top;
    char **clipboard;
    int clipboard_numrows;
    int cutting;
};
struct editorConfig E;

// file types list (to detect which filetype we are opening)

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};                             // c
char *Python_HL_extensions[] = {".py", ".pyw", NULL};                             // python
char *JS_HL_extensions[] = {".js", ".jsx", ".ts", ".tsx", ".mjs", NULL};          // javascript
char *Go_HL_extensions[] = {".go", NULL};                                         // go
char *Rust_HL_extensions[] = {".rs", NULL};                                       // rust
char *Java_HL_extensions[] = {".java", NULL};                                     // java
char *Bash_HL_extensions[] = {".sh", ".bash", ".zsh", NULL};                      // bash
char *YAML_HL_extensions[] = {".yml", ".yaml", NULL};                             // yaml
char *JSON_HL_extensions[] = {".json", NULL};                                     // json
char *Makefile_HL_extensions[] = {"Makefile", "makefile", ".mk", NULL};           // makefile
char *Docker_HL_extensions[] = {"Dockerfile", "dockerfile", ".dockerfile", NULL}; // docker
char *Gitignore_HL_extensions[] = {".gitignore", ".env", ".editorconfig", NULL};  // gitignore

// file keywords list (which keywords to highlight)

char *C_HL_keywords[] = { // c
    "switch", "if", "while", "for", "break", "continue", "return",
    "else", "struct", "union", "typedef", "static", "enum", "class",
    "case", "sizeof", "do", "extern", "inline", "const",
    "int|", "long|", "double|", "float|", "char|", "unsigned|",
    "signed|", "void|", "short|", "bool|", "size_t|", "uint8_t|",
    "uint16_t|", "uint32_t|", "uint64_t|", "int8_t|", "int16_t|",
    "int32_t|", "int64_t|",
    NULL};
char *Python_HL_keywords[] = { // python
    "and", "as", "assert", "async", "await", "break", "class",
    "continue", "def", "del", "elif", "else", "except", "finally",
    "for", "from", "global", "if", "import", "in", "is", "lambda",
    "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield",
    "True|", "False|", "None|", "int|", "str|", "float|", "bool|",
    "list|", "dict|", "tuple|", "set|", "bytes|", "type|",
    NULL};
char *JS_HL_keywords[] = { // javascript
    "break", "case", "catch", "continue", "debugger", "default",
    "delete", "do", "else", "finally", "for", "function", "if",
    "in", "instanceof", "new", "return", "switch", "this", "throw",
    "try", "typeof", "void", "while", "with", "class", "extends",
    "import", "export", "from", "of", "async", "await", "static",
    "yield", "super",
    "var|", "let|", "const|", "true|", "false|", "null|",
    "undefined|", "NaN|",
    NULL};
char *Go_HL_keywords[] = { // go
    "break", "case", "chan", "continue", "default", "defer", "else",
    "fallthrough", "for", "func", "go", "goto", "if", "import",
    "interface", "map", "package", "range", "return", "select",
    "struct", "switch", "type", "var",
    "int|", "int8|", "int16|", "int32|", "int64|", "uint|",
    "uint8|", "uint16|", "uint32|", "uint64|", "float32|",
    "float64|", "complex64|", "complex128|", "string|", "bool|",
    "byte|", "rune|", "error|", "true|", "false|", "nil|",
    NULL};
char *Rust_HL_keywords[] = { // rust
    "as", "async", "await", "break", "const", "continue", "crate",
    "dyn", "else", "enum", "extern", "fn", "for", "if", "impl",
    "in", "let", "loop", "match", "mod", "move", "mut", "pub",
    "ref", "return", "self", "Self", "static", "struct", "super",
    "trait", "type", "unsafe", "use", "where", "while",
    "bool|", "char|", "str|", "i8|", "i16|", "i32|", "i64|",
    "i128|", "isize|", "u8|", "u16|", "u32|", "u64|", "u128|",
    "usize|", "f32|", "f64|", "true|", "false|",
    NULL};
char *Java_HL_keywords[] = { // java
    "abstract", "assert", "break", "case", "catch", "class",
    "continue", "default", "do", "else", "enum", "extends",
    "final", "finally", "for", "goto", "if", "implements",
    "import", "instanceof", "interface", "native", "new", "package",
    "private", "protected", "public", "return", "static",
    "strictfp", "super", "switch", "synchronized", "this", "throw",
    "throws", "transient", "try", "void", "volatile", "while",
    "int|", "long|", "double|", "float|", "char|", "short|",
    "byte|", "boolean|", "true|", "false|", "null|",
    "String|", "Object|",
    NULL};
char *Bash_HL_keywords[] = { // bash
    "if", "then", "else", "elif", "fi", "for", "while", "do",
    "done", "case", "esac", "function", "select", "until",
    "break", "continue", "return", "exit", "in",
    "echo|", "read|", "local|", "export|", "source|", "alias|",
    "unset|", "shift|", "exec|",
    NULL};
char *YAML_HL_keywords[] = { // yaml
    "true|", "false|", "null|", "yes|", "no|", "on|", "off|",
    NULL};
char *JSON_HL_keywords[] = { // json
    "true|", "false|", "null|",
    NULL};
char *Makefile_HL_keywords[] = { // makefile
    "ifeq", "ifneq", "ifdef", "ifndef", "else", "endif",
    "include", "define", "endef", "override", "export", "unexport",
    "vpath",
    ".PHONY|", ".DEFAULT|", ".SUFFIXES|", ".DELETE_ON_ERROR|",
    NULL};
char *Docker_HL_keywords[] = { // dockerfile
    "FROM", "RUN", "CMD", "LABEL", "EXPOSE", "ENV", "ADD", "COPY",
    "ENTRYPOINT", "VOLUME", "USER", "WORKDIR", "ARG", "ONBUILD",
    "STOPSIGNAL", "HEALTHCHECK", "SHELL",
    "AS|",
    NULL};
char *Gitignore_HL_keywords[] = {NULL}; // gitignore (no keywords but added for detecting in terminal)

struct editorSyntax HLDB[] = {
    {"c",
     C_HL_extensions, C_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"python",
     Python_HL_extensions, Python_HL_keywords,
     "#", NULL, NULL,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"javascript",
     JS_HL_extensions, JS_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"go",
     Go_HL_extensions, Go_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"rust",
     Rust_HL_extensions, Rust_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"java",
     Java_HL_extensions, Java_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"bash",
     Bash_HL_extensions, Bash_HL_keywords,
     "#", NULL, NULL,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"yaml",
     YAML_HL_extensions, YAML_HL_keywords,
     "#", NULL, NULL,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"json",
     JSON_HL_extensions, JSON_HL_keywords,
     NULL, NULL, NULL,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"makefile",
     Makefile_HL_extensions, Makefile_HL_keywords,
     "#", NULL, NULL,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"dockerfile",
     Docker_HL_extensions, Docker_HL_keywords,
     "#", NULL, NULL,
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},

    {"config",
     Gitignore_HL_extensions, Gitignore_HL_keywords,
     "#", NULL, NULL,
     0},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

// prototypes (just to let the compiler know the function exists later to stop throwing errors)

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

// terminal

void die(const char *s) // display error message
{
    write(STDIN_FILENO, "\x1b[2J", 4); // clear screen
    write(STDIN_FILENO, "\x1b[H", 3);  // starts the cursor from top left corner
    perror(s);                         // display where we got error
    exit(1);
}
void disableRawMode() // default mode terminal is on
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) // error handling
        die("tcsetattr");
}
void enableRawMode() // gives complete control over terminal screen
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) // error handling
        die("tcgetattr");
    atexit(disableRawMode); // runs this right before code is exited
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | IXON | INPCK | ISTRIP); // disable input flags
    raw.c_oflag &= ~(OPOST);                                  // disable output flags
    raw.c_cflag &= ~(CS8);                                    // disable control flags
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);          // disable local flags
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) // error handling
        die("tcsetattr");
}
int editorReadKey() // read the keypress from user
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b')
    {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[')
        {
            if (seq[1] >= '0' && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5': // [5 is page up
                        return PAGE_UP;
                    case '6': // [6 is page down
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A': // A is up arrow key
                    return ARROW_UP;
                case 'B': // B is down arrow key
                    return ARROW_DOWN;
                case 'C': // C is right arrow key
                    return ARROW_RIGHT;
                case 'D': // D is left arrow key
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        else if (seq[0] == '0')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }
        return '\x1b';
    }
    else
    {
        return c;
    }
    return c;
}
int getCursorPosition(int *rows, int *cols) // gets the cursor's position
{
    char buf[32];
    unsigned int i = 0;
    if (write(STDIN_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
    editorReadKey();
    return 0;
}
int getWindowSize(int *rows, int *cols) // gets windows size
{
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if (write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
    }
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

// syntax highlighting

int is_seperator(int c)
{
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}
void editorUpdateSyntax(erow *row)
{
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);
    if (E.syntax == NULL)
        return;
    char **keywords = E.syntax->keywords;
    char *scs = E.syntax->singleline_comment_start; // single line comment
    char *mcs = E.syntax->multiline_comment_start;  // multi line comment start
    char *mce = E.syntax->multiline_comment_end;    // multi line comment end
    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;
    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
    int i = 0;
    while (i < row->rsize)
    {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;
        if (scs_len && !in_string && !in_comment)
        {
            if (!strncmp(&row->render[i], scs, scs_len))
            {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }
        if (mcs_len && mce_len && !in_string)
        {
            if (in_comment)
            {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len))
                {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                else
                {
                    i++;
                    continue;
                }
            }
            else if (!strncmp(&row->render[i], mcs, mcs_len))
            {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }
        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS)
        {
            if (in_string)
            {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize)
                {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string)
                    in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            }
            else
            {
                if (c == '"' || c == '\'')
                {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS)
        {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER))
            {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }
        if (prev_sep)
        {
            int j;
            for (j = 0; keywords[j]; j++)
            {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2)
                    klen--;
                if (!strncmp(&row->render[i], keywords[j], klen) && is_seperator(row->render[i + klen]))
                {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL)
            {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = is_seperator(c);
        i++;
    }
    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx + 1]);
}
int editorSyntaxToColor(int hl)
{
    switch (hl)
    {
    case HL_COMMENT:
    case HL_MLCOMMENT:
        return 36; // cyan
    case HL_KEYWORD1:
        return 33; // yellow
    case HL_KEYWORD2:
        return 35; // magenta
    case HL_STRING:
        return 32; // green
    case HL_NUMBER:
        return 31; // red
    case HL_MATCH:
        return 34; // blue
    default:
        return 37; // white
    }
}
void editorSelectSyntaxHighlight()
{
    E.syntax = NULL;
    if (E.filename == NULL)
        return;
    char *ext = strrchr(E.filename, '.');
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++)
    {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i])
        {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) || (!is_ext && strstr(E.filename, s->filematch[i])))
            {
                E.syntax = s;
                int filerow;
                for (filerow = 0; filerow < E.numrows; filerow++)
                {
                    editorUpdateSyntax(&E.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}
// row operations

int editorRowCxToRx(erow *row, int cx)
{
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++)
    {
        if (row->chars[j] == '\t')
            rx += (SCRIBERE_TAB_STOP - 1) - (rx % SCRIBERE_TAB_STOP);
        rx++;
    }
    return rx;
}
int editorRowRxToCx(erow *row, int rx)
{
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++)
    {
        if (row->chars[cx] == '\t')
            cur_rx += (SCRIBERE_TAB_STOP - 1) - (cur_rx % SCRIBERE_TAB_STOP);
        cur_rx++;
        if (cur_rx > rx)
            return cx;
    }
    return cx;
}

void editorUpdateRow(erow *row)
{
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
            tabs++;
    }
    free(row->render);                                                    // prevents memory leak (frees old memory)
    row->render = malloc(row->size + tabs * (SCRIBERE_TAB_STOP - 1) + 1); // +1 for the '\0' operator
    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % SCRIBERE_TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j]; // copies each char from raw file into render (display)
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) // lets us read multiple lines
{
    if (at < 0 || at > E.numrows)
        return;
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    for (int j = at + 1; j <= E.numrows; j++)
        E.row[j].idx++;
    E.row[at].idx = at;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1); // +1 to hold \0
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&E.row[at]);
    E.numrows++; // to indicate erow now contains a line that should be displayed
    E.dirty++;   // to indicate file had changes
}
void editorFreeRow(erow *row) // preventing memory leaks
{
    free(row->render);
    free(row->chars);
    free(row->hl);
}
void editorDelRow(int at)
{ // shifting the array
    if (at < 0 || at >= E.numrows)
        return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    for (int j = at + 1; j <= E.numrows; j++)
        E.row[j].idx--;
    E.numrows--;
    E.dirty++;
}
void editorRowInsertChar(erow *row, int at, int c)
{
    if (at < 0 || at > row->size)
        at = row->size;
    row->chars = realloc(row->chars, row->size + 2);                   // +2 cuz one is for the character and the other is '\0'
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1); // move the text to the right by one byte for extra space to add our character
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++; // to indicate file had changes
}
void editorRowAppendString(erow *row, char *s, size_t len)
{
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}
void editorRowDelChar(erow *row, int at)
{ // handling backspaces
    if (at < 0 || at >= row->size)
        return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at); // shift characters by once to left to prevent blank space
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}
void undoPush(ActionType type, int cy, int cx, char c)
{
    if (E.undo_top >= UNDO_STACK_SIZE)
        return; // if stack is full ignore
    E.undo_stack[E.undo_top].type = type;
    E.undo_stack[E.undo_top].cy = cy;
    E.undo_stack[E.undo_top].cx = cx;
    E.undo_stack[E.undo_top].c = c;
    E.undo_top++;
}

// editor operations

void editorInsertChar(int c)
{
    undoPush(ACTION_INSERT, E.cy, E.cx, c); // record it first before inserting
    if (E.cy == E.numrows)
    {
        editorInsertRow(E.numrows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}
void editorInsertNewLine()
{
    erow *cur = (E.cy < E.numrows) ? &E.row[E.cy] : NULL;
    int indent = 0;
    if (cur)
    {
        while (indent < cur->size && (cur->chars[indent] == ' ' || cur->chars[indent] == '\t'))
            indent++;
        if (E.cx < indent)
            indent = E.cx;
    }
    if (E.cx == 0)
    {
        editorInsertRow(E.cy, "", 0);
    }
    else
    {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
    if (indent > 0)
    {
        erow *prev = &E.row[E.cy - 1];
        for (int i = 0; i < indent; i++)
        {
            editorRowInsertChar(&E.row[E.cy], E.cx, prev->chars[i]);
            E.cx++;
        }
    }
}
void editorDelChar()
{
    if (E.cy == E.numrows)
        return;
    if (E.cx == 0 && E.cy == 0)
        return;
    erow *row = &E.row[E.cy];
    if (E.cx > 0)
    {
        undoPush(ACTION_DELETE, E.cy, E.cx - 1, row->chars[E.cx - 1]);
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    else
    {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

// file i/o

char *editorRowsToString(int *buflen)
{
    int totlen = 0;
    int j;
    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;
    char *buf = malloc(totlen);
    char *p = buf;
    for (j = 0; j < E.numrows; j++)
    {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    return buf;
}
void editorOpen(char *filename)
{
    free(E.filename); // prevents leaks
    E.filename = strdup(filename);
    editorSelectSyntaxHighlight();
    FILE *fp = fopen(filename, "r"); // opening a file into the editor
    if (!fp)
        die("fopen"); // error handling
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) // getLine takes care of the memory management
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave() // no name file (not saved)
{
    if (E.filename == NULL)
    {
        E.filename = editorPrompt("Save as: %s (Press ESC to cancel)", NULL);
    }
    if (E.filename == NULL)
    {
        editorSetStatusMessage("Save aborted");
        return;
    }
    int len;
    char *buf = editorRowsToString(&len);
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644); // 0644 are the default read and write permission
    if (fd != -1)
    {
        if (ftruncate(fd, len) != -1)
        {
            if (write(fd, buf, len) == len)
            {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
            editorSelectSyntaxHighlight();
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Can't save! I/O Error: %s", strerror(errno));
}
void editorUndo()
{
    if (E.undo_top == 0)
    {
        editorSetStatusMessage("Nothing to do.");
        return;
    }
    E.undo_top--;
    UndoAction *a = &E.undo_stack[E.undo_top];
    if (a->type == ACTION_INSERT) // whatever we character we inserted, will get removed
    {
        E.cy = a->cy;
        E.cx = a->cx;
        editorRowDelChar(&E.row[E.cy], E.cx);
    }
    else // whatever we character we deleted, will get brought back
    {
        E.cy = a->cy;
        E.cx = a->cx;
        editorRowInsertChar(&E.row[E.cy], E.cx, a->c);
    }
    editorSetStatusMessage("Undo: %s '%c' at (%d,%d)", a->type == ACTION_INSERT ? "insert" : "delete", a->c, a->cy, a->cx);
}

// find (search function)

void editorFindCallback(char *query, int key)
{
    static int last_match = -1;
    static int direction = 1;
    static int saved_hl_line;
    static char *saved_hl = NULL;
    if (saved_hl)
    {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }
    if (key == '\r' || key == '\x1b')
    {
        last_match = -1;
        direction = 1;
        return;
    }
    else if (key == ARROW_RIGHT || key == ARROW_DOWN)
    {
        direction = 1;
    }
    else if (key == ARROW_LEFT || key == ARROW_UP)
    {
        direction = -1;
    }
    else
    {
        last_match = -1;
        direction = 1;
    }
    if (last_match == -1)
        direction = 1;
    int current = last_match;
    int i;
    for (i = 0; i < E.numrows; i++)
    {
        current += direction;
        if (current == -1)
            current = E.numrows - 1;
        else if (current == E.numrows)
            current = 0;
        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if (match)
        {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;
            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}
void editorFind()
{
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;
    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
    if (query)
    {
        free(query);
    }
    else
    {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

// append buffer

struct abuf // this will let us do one single big write instead of multiple writes to prevent flickering (it will hold everything all the write's)
{
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) // dynamically reallocates the no of bytes required after appending string
{
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}
void abFree(struct abuf *ab) // it deallocates the dynamic memory used by abuf
{
    free(ab->b);
}

// output

void editorScroll()
{
    E.rx = 0;
    if (E.cy < E.numrows)
    {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }
    if (E.cy < E.rowoff)
    {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows)
    {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff)
    {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols)
    {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows)
        {
            char gutter[SCRIBERE_LINENUM_WIDTH + 1]; // so that left edge stays consistent
            snprintf(gutter, sizeof(gutter), "%*s", SCRIBERE_LINENUM_WIDTH, "");
            abAppend(ab, "\x1b[90m", 5); // gray color
            abAppend(ab, gutter, SCRIBERE_LINENUM_WIDTH);
            abAppend(ab, "\x1b[39m", 5); // default color
            if (E.numrows == 0 && y == E.screenrows / 3)
            {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Welcome to Scribere editor! Look at the bottom for commands :D"); // welcome text
                if (welcomelen > E.screencols)
                    welcomelen = E.screencols;                 // shrink welcome text if screen size is not big enough
                int padding = (E.screencols - welcomelen) / 2; // this is how to get the centre value to centre text
                if (padding)                                   // display ~ on each line to indicate lines which arent being used
                {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            }
            else
            {
                abAppend(ab, "~", 1);
            }
        }
        else
        {
            char linenum[SCRIBERE_LINENUM_WIDTH + 1];                            // draw the line no first
            int lnlen = snprintf(linenum, sizeof(linenum), "%4d ", filerow + 1); // + 1 so starts from 1
            abAppend(ab, "\x1b[90m", 5);                                         // gray color
            abAppend(ab, linenum, lnlen);
            abAppend(ab, "\x1b[39m", 5); // default color
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0)
                len = 0;
            if (len > E.screencols)
                len = E.screencols;
            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff]; // points directly to whatever is displayed on screen rn and not the things outside rendering
            int current_color = -1;
            int j;
            for (j = 0; j < len; j++)
            {
                if (iscntrl(c[j]))
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if (current_color != -1)
                    {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                }
                else if (hl[j] == HL_NORMAL)
                {
                    if (current_color != -1)
                    {
                        abAppend(ab, "\x1b[39m", 5); // change it back to normal
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1); // only digit will turn to red color
                }
                else
                {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color)
                    {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1); // append normally with default color
                }
            }
            abAppend(ab, "\x1b[39m", 5);
        }
        abAppend(ab, "\x1b[K", 3); // erases part of line
        abAppend(ab, "\r\n", 2);
    }
}
void editorDrawStatusBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No filename]", E.numrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no filetype", E.cy + 1, E.numrows); // which line we are currently on and file type
    if (len > E.screencols)
        len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols)
    {
        if (E.screencols - len == rlen)
        {
            abAppend(ab, rstatus, rlen);
            break;
        }
        else
        {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}
void editorDrawMessageBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[K", 3); // clears message bar
    if (E.msg_is_permanent)    // permanent message
    {
        int msglen = strlen(E.statusmsg);
        if (msglen > E.screencols)
            msglen = E.screencols;
        abAppend(ab, E.statusmsg, msglen);
        return;
    }

    int msglen = strlen(E.statusmsg); // temporary message (5 seconds)
    if (msglen > E.screencols)
        msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
    {
        abAppend(ab, E.statusmsg, msglen);
    }
    else
    {
        const char *hint = "HELP: Ctrl-Q = quit | Ctrl-S = save | Ctrl-F = find | Ctrl-Z = undo | Ctrl-K = cut | Ctrl-U = paste";
        int hintlen = strlen(hint);
        if (hintlen > E.screencols)
            hintlen = E.screencols;
        abAppend(ab, hint, hintlen);
    }
}
void editorRefreshScreen() // clearing the screen (2 represents clearing the whole screen in below line)
{
    editorScroll();
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // hide cursor before refreshing (l is hide cursor) (cursor might flicker thats why)
    abAppend(&ab, "\x1b[H", 3);    // starts the cursor from top left corner
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1 + SCRIBERE_LINENUM_WIDTH); // move cursor to position in cx and cy
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6); // show cursor since refreshing is done (h is show cursor)
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}
void editorSetStatusMessage(const char *fmt, ...)
{
    E.msg_is_permanent = 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}
void editorSetPermanentMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
    E.msg_is_permanent = 1;
}
// input

char *editorPrompt(char *prompt, void (*callback)(char *, int))
{
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';
    while (1)
    {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();
        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
        {
            if (buflen != 0)
                buf[--buflen] = '\0';
        }
        else if (c == '\x1b')
        {
            editorSetStatusMessage("");
            if (callback)
                callback(buf, c);
            free(buf);
            return NULL;
        }
        else if (c == '\r')
        {
            if (buflen != 0)
            {
                editorSetStatusMessage("");
                if (callback)
                    callback(buf, c);
                return buf;
            }
        }
        else if (!iscntrl(c) && c < 128)
        {
            if (buflen == bufsize - 1)
            {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
        if (callback)
            callback(buf, c);
    }
}
void editorMoveCursor(int key)
{
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0) // prevents from cursor going out of screen
        {
            E.cx--;
        }
        else if (E.cy > 0) // go to previous line when press <- at start of line
        {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size)
        {
            E.cx++;
        }
        else if (row && E.cx == row->size) // go to next line when press -> at end of line
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
        {
            E.cy--;
        }
        break;
    case ARROW_DOWN:
        if (E.cy < E.numrows)
        {
            E.cy++;
        }
        break;
    }
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen)
    {
        E.cx = rowlen;
    }
}
void editorCutRow()
{
    if (!E.cutting) // if starting fresh, empty the clipboard
    {
        for (int i = 0; i < E.clipboard_numrows; i++)
            free(E.clipboard[i]);
        free(E.clipboard);
        E.clipboard = NULL;
        E.clipboard_numrows = 0;
    }
    E.clipboard = realloc(E.clipboard, sizeof(char *) * (E.clipboard_numrows + 1)); // grow the array by one slot
    erow *row = &E.row[E.cy];                                                       // copy this line's text into the new slot
    E.clipboard[E.clipboard_numrows] = malloc(row->size + 1);
    memcpy(E.clipboard[E.clipboard_numrows], row->chars, row->size);
    E.clipboard[E.clipboard_numrows][row->size] = '\0';
    E.clipboard_numrows++;
    editorDelRow(E.cy); // delete the row from the file
    if (E.cy >= E.numrows && E.cy > 0)
        E.cy--;
    E.cutting = 1; // inform that we are in a cut sequence right now
    editorSetStatusMessage("%d line(s) in clipboard", E.clipboard_numrows);
}
void editorPasteRows()
{
    if (!E.clipboard || E.clipboard_numrows == 0)
    {
        editorSetStatusMessage("Clipboard is empty.");
        return;
    }
    for (int i = 0; i < E.clipboard_numrows; i++) // insert each clipboard line above the current row
        editorInsertRow(E.cy + 1, E.clipboard[i], strlen(E.clipboard[i]));
    E.cy += E.clipboard_numrows; // move cursor to the last pasted line
    E.cx = 0;
    editorSetStatusMessage("%d line(s) pasted", E.clipboard_numrows);
}
void editorProcessKeypress() // reads the keypress then handles it
{
    static int quit_times = SCRIBERE_QUIT_TIMES;
    int c = editorReadKey();
    if (c != CTRL_KEY('k')) // reset cut sequence on any non ctrl-k keypresses
        E.cutting = 0;
    switch (c)
    {
    case '\r':
        editorInsertNewLine();
        break;
    case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0)
        {
            editorSetStatusMessage("WARNING! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
            quit_times--;
            return;
        }
        write(STDIN_FILENO, "\x1b[2J", 4); // clear screen
        write(STDIN_FILENO, "\x1b[H", 3);  // starts the cursor from top left corner
        exit(0);
        break;
    case CTRL_KEY('s'):
        editorSave();
        break;
    case CTRL_KEY('z'):
        editorUndo();
        break;
    case CTRL_KEY('k'):
        editorCutRow();
        break;
    case CTRL_KEY('u'):
        editorPasteRows();
        break;
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        if (E.cy < E.numrows)
            E.cx = E.row[E.cy].size;
        break;
    case CTRL_KEY('f'):
        editorFind();
        break;
    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
        if (c == DEL_KEY)
            editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    {
        if (c == PAGE_UP)
        {
            E.cy = E.rowoff;
        }
        else if (c == PAGE_DOWN)
        {
            E.cy = E.rowoff + E.screenrows - 1;
            if (E.cy > E.numrows)
                E.cy = E.numrows;
        }
        int times = E.screenrows;
        while (times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    case CTRL_KEY('l'):
    case '\x1b':
        break;
    default:
        editorInsertChar(c);
        break;
    }
    quit_times = SCRIBERE_QUIT_TIMES;
}

// init (this happens first after boot/launching)

void initEditor()
{
    E.cx = 0;     // row pos is 0 at start
    E.cy = 0;     // col pos is 0 at start
    E.rx = 0;     // used for handling tab spaces
    E.rowoff = 0; // scrolled to top at start
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL; // so that its empty when no file is open
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.msg_is_permanent = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) // error handling
        die("getWindowSize");
    E.screenrows -= 2;                      // no of lines to leave for displaying status bar
    E.screencols -= SCRIBERE_LINENUM_WIDTH; // no of lines to leave for no indexing
    E.syntax = NULL;
    E.clipboard = NULL;
    E.clipboard_numrows = 0;
    E.cutting = 0;
}

int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }
    editorSetPermanentMessage("HELP: Ctrl-Q = quit | Ctrl-S = save | Ctrl-F = find | Ctrl-Z = undo | Ctrl-K = cut | Ctrl-U = paste");
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
