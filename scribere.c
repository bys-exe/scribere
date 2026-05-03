// header files

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// defined

#define CTRL_KEY(k) ((k) & 0x1f) // ctrl key shorthand

// data

struct termios orig_termios;

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}
void enableRawMode() // gives complete control over terminal screen
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode); // runs this right before code is exited
    struct termios raw = orig_termios;
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

// output

void editorDrawRows()
{ // display ~ to indicate lines which arent being used
    int y;
    for (y = 0; y < 24; y++)
    {
        write(STDIN_FILENO, "~\r\n", 3);
    }
}
void editorRefreshScreen() // clearing the screen (2 represents clearing the whole screen in below line)
{
    write(STDIN_FILENO, "\x1b[2J", 4); // \x1b is escape character (1 byte), [ 2 J are 1 byte each respectively
    write(STDIN_FILENO, "\x1b[H", 3);  // starts the cursor from top left corner
    editorDrawRows();
    write(STDIN_FILENO, "\x1b[H", 3);
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
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
