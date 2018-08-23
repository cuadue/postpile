ifeq ($(shell uname), Darwin)
all: Postpile.app
LDFLAGS += -framework OpenGL
else
all: postpile
endif

PKGS = glew glfw3

CFLAGS += -Werror -Wall -Wextra -std=c99 -O1

CXXFLAGS += -Werror -Wall -Wextra -std=c++11 -O1
CXXFLAGS += $(shell pkg-config --static --cflags $(PKGS))

LDFLAGS += $(shell pkg-config --static --libs $(PKGS))
LDFLAGS += -lm

OBJS = postpile.o wavefront.o wavefront_mtl.o hex.o
OBJS += tiles.o osn.o time.o render_obj.o
OBJS += gl3.o gl3_aux.o gl_aux.o lmdebug.o depthmap.o
OBJS += stb_image.o intersect.o

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
	mkdir -p $@/Contents/MacOS/Resources
	cp -a postpile $@/Contents/MacOS/_postpile
	cp -a mac/bootstrap.sh $@/Contents/MacOS/postpile
	cp -a mac/Info.plist $@/Contents
	cp -a mac/PkgInfo $@/Contents
	cp -a mac/*.icns $@/Contents/MacOS
	cp -a *.obj tex $@/Contents/MacOS
	cp -a *.vert *.frag $@/Contents/MacOS
	cp -afLH "$$(otool -L postpile | awk '/glew/ {print $$1}')" $@/Contents/MacOS
	cp -afLH "$$(otool -L postpile | awk '/glfw/ {print $$1}')" $@/Contents/MacOS

.PHONY: sign
sign: Postpile.app
	codesign -i "A Pile of Posts" -v -f --deep \
		-s "Mac Developer: wwaugh@gmail.com (Q9A8EM6CP3)" \
		Postpile.app/Contents/MacOS/{_postpile,postpile,*.dylib}
	codesign -v Postpile.app
endif

clean:
	rm -f $(OBJS)
	rm -f postpile
	rm -rf tex
	rm -rf Postpile.app
