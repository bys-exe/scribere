// header files

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// defined

#define CTRL_KEY(k) ((k) & 0x1f) // ctrl key shorthand

// data

struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};
struct editorConfig E;

// terminal

void die(const char *s) // display error message
{
    write(STDIN_FILENO, "\x1b[2J", 4); // clear screen
    write(STDIN_FILENO, "\x1b[H", 3);  // starts the cursor from top left corner
    perror(s);
    exit(1);
}
void disableRawMode() // default mode terminal is on
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}
void enableRawMode() // gives complete control over terminal screen
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode); // runs this right before code is exited
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | IXON | INPCK | ISTRIP); // disable input flags
    raw.c_oflag &= ~(OPOST);                                  // disable output flags
    raw.c_cflag &= ~(CS8);                                    // disable control flags
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);          // disable local flags
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}
char editorReadKey() // read the keypress from user
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
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
void initEditor()
{
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

// append buffer

struct abuf // this will let us do one single big write instead of multiple writes to prevent flickering (it will hold everything all the write's)
{
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}

// output

void editorDrawRows() // display ~ to indicate lines which arent being used
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        write(STDOUT_FILENO, "~", 1);
        if (y < E.screenrows - 1)
        {
            write(STDOUT_FILENO, "\r\n", 2);
        }
    }
}
void editorRefreshScreen() // clearing the screen (2 represents clearing the whole screen in below line)
{
    write(STDOUT_FILENO, "\x1b[2J", 4); // \x1b is escape character (1 byte), [ 2 J are 1 byte each respectively
    write(STDOUT_FILENO, "\x1b[H", 3);  // starts the cursor from top left corner
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

// input

void editorProcessKeypress() // reads the keypress then handles it
{
    char c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDIN_FILENO, "\x1b[2J", 4); // clear screen
        write(STDIN_FILENO, "\x1b[H", 3);  // starts the cursor from top left corner
        exit(0);
        break;
    }
}

// init (this happens first after boot/launching)

int main()
{
    enableRawMode();
    initEditor();
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
