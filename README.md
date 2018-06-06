# choice
A small console based menu selector, inspired by dmenu, written in bash

Reads STDIN, splits each line around the first space, the left part is the key, the right part is the value.

Displays the values in an interactive menu, and prints the key of the selected value in STDOUT.

If there is no space in a line, the value is interpreted as both the key and the value.

**Requires the color.sh, keyboard.sh and cursor.sh scripts (https://framagit.org/fxcarton/bash-scripts) in the PATH**
