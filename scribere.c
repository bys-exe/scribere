// header files

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// defined

#define CTRL_KEY(k) ((k) & 0x1f) // ctrl key shorthand
#define SCRIBERE_VERSION "0.0.1"
#define KILO_TAB_STOP 8
enum editorKey
{
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

// data

typedef struct erow // editor row
{
    int size;
    int rsize;
    char *chars;
    char *render; // prevents extra spaces in terminal
} erow;

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
    struct termios orig_termios;
};
struct editorConfig E;

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

// row operations

int editorRowCxToRx(erow *row, int cx)
{
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++)
    {
        if (row->chars[j] == '\t')
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        rx++;
    }
    return rx;
}

void editorUpdateRow(erow *row)
{
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
            tabs++;
        free(row->render);                                                // prevents memory leak (frees old memory)
        row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1); // +1 for the '\0' operator
    }
    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % KILO_TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j]; // copies each char from raw file into render (display)
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) // lets us read multiple lines
{
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1); // +1 to hold \0
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    E.numrows++; // to indicate erow now contains a line that should be displayed
}

// file i/o

void editorOpen(char *filename)
{
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
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
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

void editorDrawRows(struct abuf *ab) // display ~ on each line to indicate lines which arent being used
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows)
        {
            if (E.numrows == 0 && y == E.screenrows / 3)
            {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Scribere editor -- v%s", SCRIBERE_VERSION); // welcome text
                if (welcomelen > E.screencols)
                    welcomelen = E.screencols;                 // shrink welcome text if screen size is not big enough
                int padding = (E.screencols - welcomelen) / 2; // this is how to get the centre value to centre text
                if (padding)
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
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0)
                len = 0;
            if (len > E.screencols)
                len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }
        abAppend(ab, "\x1b[K", 3); // erases part of line
        if (y < E.screenrows - 1)
        {
            abAppend(ab, "\r\n", 2);
        }
    }
}
void editorRefreshScreen() // clearing the screen (2 represents clearing the whole screen in below line)
{
    editorScroll();
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // hide cursor before refreshing (l is hide cursor) (cursor might flicker thats why)
    abAppend(&ab, "\x1b[H", 3);    // starts the cursor from top left corner
    editorDrawRows(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1); // move cursor to position in cx and cy
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6); // show cursor since refreshing is done (h is show cursor)
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// input

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

void editorProcessKeypress() // reads the keypress then handles it
{
    int c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDIN_FILENO, "\x1b[2J", 4); // clear screen
        write(STDIN_FILENO, "\x1b[H", 3);  // starts the cursor from top left corner
        exit(0);
        break;
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screencols - 1;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    {
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
    }
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
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) // error handling
        die("getWindowSize");
}

int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if (argc >= 2)
    {
        editorOpen(argv[1]);
    }
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
