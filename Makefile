all: postpile

ifeq ($(shell uname), Darwin)
LDFLAGS += -framework OpenGL
PKGS = glew
else
PKGS = glew x11 xrandr xxf86vm gl
endif

CFLAGS += -Werror -Wall -Wextra -std=c99 -ggdb

CXXFLAGS += -Werror -Wall -Wextra -std=c++11 -ggdb
CXXFLAGS += $(shell sdl2-config --cflags)
CXXFLAGS += $(shell pkg-config --static --cflags $(PKGS))

LDFLAGS += $(shell sdl2-config --libs) -lSDL2_ttf -lSDL2_image
LDFLAGS += -lm
LDFLAGS += $(shell pkg-config --static --libs $(PKGS))


OBJS = postpile.oo wavefront.oo wavefront_mtl.oo gl2.oo hex.o
OBJS += tiles.o osn.o
.PHONY: postpile.hpp

%.oo: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

postpile: $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f $(OBJS)
	rm -f postpile
