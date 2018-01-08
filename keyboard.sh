read_key_no_stty() {
    IFS="" read -n1 -r $(test -z "$1" || printf -- '-t%s' "$1") KEY < /dev/tty
    if [ "$KEY" == "" ]; then
        read -n1 -r KEY < /dev/tty
        if [ "$KEY" == "[" ]; then
            KEY="ESC"
            read -n1 -r KEY < /dev/tty
            case "$KEY" in
                A) KEY="UP" ;;
                B) KEY="DOWN" ;;
                C) KEY="RIGHT" ;;
                D) KEY="LEFT" ;;
                F) KEY="END" ;;
                H) KEY="ORIG" ;;
            esac
        fi
    elif [ "$KEY" == "" ]; then
        KEY="BACKSPACE"
    elif [ "$KEY" == " " ]; then
        KEY="SPACE"
    fi
}

read_key() {
    #stty -echo
    read_key_no_stty "$@"
    #stty echo
}
