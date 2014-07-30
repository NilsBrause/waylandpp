#!/bin/sh
if [ ! -e wayland.xml ]
then
    wget -c http://cgit.freedesktop.org/wayland/wayland/plain/protocol/wayland.xml
fi
g++ -ggdb -Wall -std=c++11 -I. main.cpp pugixml.cpp -omain || return 1
./main || return 1
g++ -Wall -Werror -std=c++11 -I. -DWAYLAND test.cpp core.cpp wayland.cpp egl.cpp -lwayland-client -lwayland-egl -lGL -lEGL || return 1
