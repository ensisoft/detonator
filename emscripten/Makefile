
all: test_engine

test_engine: stb_lib
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
	stb_image.o \
	stb_image_write.o \
	-s WASM=1 \
	-s FORCE_FILESYSTEM=1 \
	-s USE_HARFBUZZ=1 \
	-s USE_FREETYPE=1 \
	-s USE_BOOST_HEADERS=1 \
	-o test-engine.js

stb_lib:
	emcc \
	-O3 \
	../third_party/stb/stb_image.c \
	../third_party/stb/stb_image_write.c \
	-c \
