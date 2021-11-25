
all: test_engine

BUILD_DIR := .

BOX2D_SRC_DIR := ../third_party/box2d/src

# Find all the C and C++ files we want to compile
# Note the single quotes around the * expressions. Make will incorrectly expand these otherwise.
BOX2D_SRCS := $(shell find $(BOX2D_SRC_DIR) -name '*.cpp')

BOX2D_OBJS := $(BOX2D_SRCS:%=$(BUILD_DIR)/%.o)

BOX2D: $(BOX2D_OBJS)
	emcc $(BOX2D_OBJS) -r -o box2d.o

%.cpp.o: %.cpp
	emcc -O3 -std=c++17 -I../third_party/box2d/include -I../third_party/box2d/src -c

test_engine: stb_lib BOX2D
	emcc \
	-O3 \
	-std=c++17 \
	-fbracket-depth=512 \
	-fexceptions \
	-I. \
	-I../ \
	-I../wdk \
	-I../third_party/ \
	-I../third_party/glm \
	-I../third_party/box2d/include \
	stb.o \
	box2d.o \
	main.cpp \
	../base/assert.cpp \
	../base/logging.cpp \
	../base/utility.cpp \
	../base/format.cpp \
	../base/json.cpp \
	../base/trace.cpp \
	../game/animation.cpp \
	../game/entity.cpp \
	../game/scene.cpp \
	../game/scriptvar.cpp \
	../engine/test/main.cpp \
	../engine/renderer.cpp \
	../engine/ui.cpp \
	../engine/physics.cpp \
	../graphics/painter.cpp \
	../graphics/material.cpp \
	../graphics/drawable.cpp \
	../graphics/image.cpp \
	../graphics/loader.cpp \
	../graphics/drawing.cpp \
	../graphics/bitmap.cpp \
	../graphics/text.cpp \
	../graphics/opengl_es2_device.cpp \
	../uikit/window.cpp \
	../uikit/widget.cpp \
	../third_party/base64/base64.cpp \
	-s WASM=1 \
	-s FORCE_FILESYSTEM=1 \
	-s USE_HARFBUZZ=1 \
	-s USE_FREETYPE=1 \
	-s USE_BOOST_HEADERS=1 \
	-o test-engine.js

stb_lib:
	emcc -O3 ../third_party/stb/stb_image.c -c
	emcc -O3 ../third_party/stb/stb_image_write.c -c
	emcc stb_image.o stb_image_write.o -r -o stb.o
