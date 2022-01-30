# Copyright (c) 2014-2022 Nils Christopher Brause, Philipp Kerling
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Werror -ggdb -O2 `pkg-config --cflags --libs ${LIBS}`
SRC = egl.cpp shm.cpp dump.cpp proxy_wrapper.cpp foreign_display.cpp

all: $(patsubst %.cpp,%,${SRC})

egl: LIBS = wayland-client++ wayland-client-extra++ wayland-cursor++ wayland-egl++
egl: FLAGS = -lEGL -lGL
shm: LIBS = wayland-client++ wayland-client-extra++ wayland-cursor++
shm: FLAGS = -lrt
dump: LIBS = wayland-client++
proxy_wrapper: LIBS = wayland-client++
proxy_wrapper: FLAGS = -pthread
foreign_display: LIBS = wayland-client++

%: %.cpp Makefile
	${CXX} $< ${CXXFLAGS} ${FLAGS} -o $@

clean: Makefile
	rm -f $(patsubst %.cpp,%,${SRC})

.SUFFIXES:
