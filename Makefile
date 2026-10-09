# Duffer Makefile
# Portable C99 Makefile supporting MinGW (Windows), Linux, and macOS

CC = gcc
CFLAGS ?= -std=c99 -Wall -Wextra -O2

SRCS = src/golf_core.c src/golf_render.c src/main.c
OBJS = src/golf_core.o src/golf_render.o src/main.o

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
	@echo Built target: $(TARGET)

clean:
	@if exist src\*.o del /Q /F src\*.o
	@if exist build\duffer.exe del /Q /F build\duffer.exe
	@if exist build\retrosimgolf.exe del /Q /F build\retrosimgolf.exe
else
$(TARGET): $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo Built target: $(TARGET)

clean:
	@rm -f src/*.o build/duffer build/duffer.exe build/retrosimgolf build/retrosimgolf.exe
endif

.PHONY: all clean
