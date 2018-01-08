#!/bin/bash

. color.sh
. cursor.sh
. keyboard.sh

declare -A STDIN

print_menu() {
    erase_display 2 > /dev/tty
    cursor_pos 0 0 > /dev/tty

    for i in $(seq 0 $((${#STDIN[@]}-1))) ; do
        ENTRY="$(echo  "${STDIN[$i]}" | cut -d" " -f2-)"
        [ "$i" == "$1" ] && set_bg $COLOR_WHITE > /dev/tty && set_fg $COLOR_BLACK > /dev/tty
        printf "$ENTRY" > /dev/tty
        reset_fg > /dev/tty
        reset_bg > /dev/tty
        printf "\n" > /dev/tty
    done
}

select_entry() {
    entry=0
    size=${#STDIN[@]}
    while true ; do
        print_menu "$entry"
        read_key
        case $KEY in
            UP)     entry=$(((entry-1+size)%size)) ;;
            DOWN)   entry=$(((entry+1)%size)) ;;
        esac

        if [ -z "$KEY" ] ; then
            echo "$(echo "${STDIN[$entry]}" | cut -d" " -f1)"
            exit
        fi
    done
}
        

index=0

while read LINE ; do
    STDIN[$index]="$LINE"
    ((index++))
done

select_entry
