#!/bin/bash

clear
while read
do
    cfiles="$(ls ../src/*.c)"
    hfiles="$(ls ../src/*.h)"

    clear
    echo " │"
    echo " ├── Source files:"
    for i in "$cfiles"; do
        printf " │   └──%s\n" $i
    done
    echo " └── Headers:"
    for i in "$hfiles"; do
        printf "     └──%s\n" $i
    done
done < <(inotifywait -m -e close_write -e delete "../src")
