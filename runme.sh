#!/bin/sh
g++ -ggdb -Wall -std=c++11 -I. main.cpp pugixml.cpp -omain &&
wget -c http://cgit.freedesktop.org/wayland/wayland/plain/protocol/wayland.xml &&
cat wayland.xml | ./main > waylandpp.cpp &&
g++ -Wall -Werror -std=c++11 -I. test.cpp -lwayland-client
