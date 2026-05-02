## 2nd May 2026
[1] finding which project to choose
- first i found how to make a text editor in build your own x github repo
- started following it and started scribere, my own text editor
---
[2] things i did
- implemented make build system to make compiling simpler
- add a function to enable raw mode (opposite to canonical mode which is default), it lets us type each character on the screen simultaneously instead of it showing up only after we click enter in the terminal (i control the screen). We can achieve this by disabling some flags using NOT bitwise operator.
- display the characters on the screen alongside the ascii values for now.
- disabled all the ctrl commands so i have a blank slate which completes making raw mode.
- added error handling
---
