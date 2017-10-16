CXX = g++
CXXFLAGS = -std=c++11 -Wall -Werror -ggdb -O2 `pkg-config --cflags --libs ${LIBS}`
SRC = opengles.cpp shm.cpp dump.cpp proxy_wrapper.cpp

all: $(patsubst %.cpp,%,${SRC})

opengles: LIBS = wayland-client++ wayland-cursor++ wayland-egl++
opengles: FLAGS = -lEGL -lGL
shm: LIBS = wayland-client++ wayland-cursor++
shm: FLAGS = -lrt
dump: LIBS = wayland-client++
proxy_wrapper: LIBS = wayland-client++
proxy_wrapper: FLAGS = -pthread

%: %.cpp Makefile
	${CXX} $< ${CXXFLAGS} ${FLAGS} -o $@

Clean: Makefile
	rm -f $(patsubst %.cpp,%,${SRC})

.SUFFIXES:
