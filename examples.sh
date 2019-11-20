# Bash history search (C-r) replacement, using choice:
bind -x '"\C-r": history|sed "s/^ *[0-9]* *//"|(printf "\\x1b\\x5b\\x34\\x7e\\x15";choice -s "" -r "%k" -S "$READLINE_LINE")|perl -e "open(my \$f,\"<\",\"/dev/tty\");while(<>){ioctl(\$f,0x5412,\$_)for split \"\"}"'

# grep + choice + vim:
vgrep() {
    r="$(grep -Hn "$@" | sed 's/:/+/' | choice -s : -d "$(printf '\x1B[32m%%k\x1B[0m: %%v')")" && vim "$(cut -d+ -f1 <<< "$r")" "+$(cut -d+ -f2 <<< "$r")"
}

# find + choice + vim
vfind() {
    r="$(find "$@" | choice -s '')" && vim "$r"
}

# git log + choice + git show
glogshow() {
    r="$(git log --oneline "$@" | choice)" && git show "$r"
}

# youtube-dl formats
ytdl_format() {
    r="$(youtube-dl --list-formats "$@" | sed -n '/^format /,$p' | sed '1d;s/  */ /' | choice)" && youtube-dl -f "$r" "$@"
}
