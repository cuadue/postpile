all: postpile

ifeq ($(shell uname), Darwin)
LDFLAGS += -framework OpenGL
PKGS = glew
else
PKGS = glew x11 xrandr xxf86vm gl
endif

CFLAGS += -Werror -Wall -Wextra -std=c99 -O1

CXXFLAGS += -Werror -Wall -Wextra -std=c++11 -O1
CXXFLAGS += $(shell sdl2-config --cflags)
CXXFLAGS += $(shell pkg-config --static --cflags $(PKGS))

LDFLAGS += $(shell sdl2-config --libs) -lSDL2_ttf -lSDL2_image
LDFLAGS += -lm
LDFLAGS += $(shell pkg-config --static --libs $(PKGS))


OBJS = postpile.o wavefront.o wavefront_mtl.o gl2.o hex.o
OBJS += tiles.o osn.o

tex:
	mkdir -p $@
	cd img; for f in ./* \
	;do convert "$$f" -resize 256x256 -sigmoidal-contrast '4,50%' "../$@/$$f" \
	;done

postpile.o: postpile.cpp fir_filter.hpp
%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

postpile: $(OBJS) tex
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) 

clean:
	rm -f $(OBJS)
	rm -f postpile
	rm -rf tex
