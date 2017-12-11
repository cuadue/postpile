all: postpile

ifeq ($(shell uname), Darwin)
LDFLAGS += -framework OpenGL
PKGS = glew glfw3
else
PKGS = glfw3 glew x11 xrandr xxf86vm gl
endif

CFLAGS += -Werror -Wall -Wextra -std=c99 -O1
CFLAGS += -Iglm

CXXFLAGS += -Werror -Wall -Wextra -std=c++11 -O1
CXXFLAGS += $(shell sdl2-config --cflags)
CXXFLAGS += $(shell pkg-config --static --cflags $(PKGS))
CXXFLAGS += -Iglm

LDFLAGS += $(shell sdl2-config --libs) -lSDL2_ttf -lSDL2_image
LDFLAGS += -lm
LDFLAGS += $(shell pkg-config --static --libs $(PKGS))


OBJS = postpile.o wavefront.o wavefront_mtl.o hex.o
OBJS += tiles.o osn.o time.o
OBJS += gl3.o gl3_aux.o gl_aux.o

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

ifeq ($(shell uname), Darwin)
Postpile.app: postpile tex post.obj
	mkdir -p $@/Contents/MacOS
	cp -a postpile $@/Contents/MacOS/_postpile
	cp -a osx_bootstrap.sh $@/Contents/MacOS/postpile
	cp -a *.obj tex $@/Contents/MacOS
endif

clean:
	rm -f $(OBJS)
	rm -f postpile
	rm -rf tex
