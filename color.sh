COLOR_BLACK=0
COLOR_RED=1
COLOR_GREEN=2
COLOR_YELLOW=3
COLOR_BLUE=4
COLOR_MAGENTA=5
COLOR_CYAN=6
COLOR_WHITE=7
COLOR_CUSTOM=8
COLOR_DEFAULT=9

COLOR_DARK=0
COLOR_BRIGHT=60

COLOR_PALETTED=5
COLOR_RGB=2

reset_formats() {
    printf '\e[0m'
}

set_bold() {
    printf '\e[1m'
}
reset_bold() {
    printf '\e[22m'
}

set_italic() {
    printf '\e[3m'
}
reset_italic() {
    printf '\e[23m'
}

color_negative() {
    printf '\e[7m'
}
color_positive() {
    printf '\e[27m'
}

# set_bg $COLOR_RED [$COLOR_BRIGHT]
# set_bg $COLOR_CUSTOM $COLOR_RGB r g b
# set_bg $COLOR_CUSTOM $COLOR_PALETTED n
set_bg() {
    if [ "$1" -eq "$COLOR_CUSTOM" ]; then
        shift
        printf '\e[48'
        printf ';%d' "$@"
        printf 'm'
    else
        printf '\e[%d%sm' "$((40 + $1 + ${2:-0}))"
    fi
}
reset_bg() {
    printf '\e[49m'
}

# set_fg $COLOR_RED [$COLOR_BRIGHT]
# set_fg $COLOR_CUSTOM $COLOR_RGB r g b
# set_fg $COLOR_CUSTOM $COLOR_PALETTED n
set_fg() {
    if [ "$1" -eq "$COLOR_CUSTOM" ]; then
        shift
        printf '\e[38'
        printf ';%d' "$@"
        printf 'm'
    else
        printf '\e[%d%sm' "$((30 + $1 + ${2:-0}))"
    fi
}
reset_fg() {
    printf '\e[39m'
}

reset_colors() {
    reset_fg; reset_bg
}

# set_palette n r g b
set_palette() {
    [ "$TERM" = "linux" ] && printf '\e]P%d%02X%02X%02X' "$1" "$2" "$3" "$4"
}

reset_palette() {
    [ "$TERM" = "linux" ] && printf '\e]R' "$1" "$2" "$3" "$4"
}
