#!/bin/sh
if [ ! -e wayland.xml ]
then
    wget -c http://cgit.freedesktop.org/wayland/wayland/plain/protocol/wayland.xml
fi
g++ -ggdb -Wall -std=c++11 -I. main.cpp pugixml.cpp -omain || return 1
./main || return 1
g++ -Wall -Werror -std=c++11 -I. test.cpp wayland.cpp -lwayland-client || return 1
