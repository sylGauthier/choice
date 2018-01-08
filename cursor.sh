cursor_up() {
    printf '\e[%sA' "$1"
}

cursor_down() {
    printf '\e[%sB' "$1"
}

cursor_forward() {
    printf '\e[%sC' "$1"
}

cursor_back() {
    printf '\e[%sD' "$1"
}

cursor_next_line() {
    printf '\e[%sE' "$1"
}

cursor_prev_line() {
    printf '\e[%sF' "$1"
}

cursor_col() {
    printf '\e[%sG' "$1"
}

cursor_pos() {
    printf '\e[%s;%sH' "$1" "$2"
}

erase_display() {
    printf '\e[%sJ' "$1"
}

erase_in_line() {
    printf '\e[%sK' "$1"
}

reset() {
    printf '\ec'
}

cursor_save() {
    printf '\e[s'
}

cursor_restore() {
    printf '\e[u'
}

cursor_get() (
    stty -echo
    printf '\e[6n' > /dev/tty
    read -d R _val
    stty echo
    echo "$_val" | tail -c +3
)

cursor_hide() {
    printf '\e[?25l'
}

cursor_show() {
    printf '\e[?25h'
}
