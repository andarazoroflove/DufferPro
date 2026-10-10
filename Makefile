# Duffer-Pro Makefile
# Portable C99 Makefile supporting MinGW (Windows), Linux, and macOS

CC = gcc
CFLAGS ?= -std=c99 -Wall -Wextra -O2

SRCS = src/golf_core.c src/golf_render.c src/golf_audio.c src/main.c
OBJS = src/golf_core.o src/golf_render.o src/golf_audio.o src/main.o

ifeq ($(OS),Windows_NT)
    TARGET = build/duffer.exe
    ifneq ($(wildcard vendor/SDL2/include/SDL2/SDL.h),)
        INCLUDES = -Isrc -Ivendor/SDL2/include
        LDFLAGS = -Lvendor/SDL2/lib -lmingw32 -lSDL2main -lSDL2 -mwindows
    else
        INCLUDES = -Isrc $(shell sdl2-config --cflags 2>nul)
        LDFLAGS = $(shell sdl2-config --libs 2>nul) -mwindows
    endif
else
    TARGET = build/duffer
    INCLUDES = -Isrc $(shell sdl2-config --cflags)
    LDFLAGS = $(shell sdl2-config --libs) -lm
endif

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

all: $(TARGET)

ifeq ($(OS),Windows_NT)
$(TARGET): $(OBJS)
	@if not exist build mkdir build
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@if exist vendor\SDL2\bin\SDL2.dll copy /Y vendor\SDL2\bin\SDL2.dll build\SDL2.dll >nul
	@copy /Y build\duffer.exe build\duffer_pro.exe >nul
	@echo Built targets: build/duffer.exe and build/duffer_pro.exe

clean:
	@if exist src\*.o del /Q /F src\*.o
	@if exist build\duffer.exe del /Q /F build\duffer.exe
	@if exist build\duffer_pro.exe del /Q /F build\duffer_pro.exe
	@if exist build\retrosimgolf.exe del /Q /F build\retrosimgolf.exe
else
$(TARGET): $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@cp -f build/duffer build/duffer_pro 2>/dev/null || true
	@echo Built target: $(TARGET)

clean:
	@rm -f src/*.o build/duffer build/duffer.exe build/duffer_pro build/duffer_pro.exe build/retrosimgolf build/retrosimgolf.exe
endif

.PHONY: all clean
