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

# GBA Bare-Metal ARM Toolchain
ifneq ($(wildcard C:/Users/adam/arm-toolchain/bin/arm-none-eabi-gcc.exe),)
    ARM_GCC ?= C:/Users/adam/arm-toolchain/bin/arm-none-eabi-gcc.exe
    ARM_OBJCOPY ?= C:/Users/adam/arm-toolchain/bin/arm-none-eabi-objcopy.exe
else
    ARM_GCC ?= arm-none-eabi-gcc
    ARM_OBJCOPY ?= arm-none-eabi-objcopy
endif

ifeq ($(OS),Windows_NT)
    GBAFIX = build/gbafix.exe
else
    GBAFIX = build/gbafix
endif

gba: $(GBAFIX)
ifeq ($(OS),Windows_NT)
	@if not exist build mkdir build
	"$(ARM_GCC)" -std=c99 -O2 -mcpu=arm7tdmi -mthumb-interwork -marm -ffreestanding -fno-builtin -fno-common -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib -Isrc -Igba gba/crt0.s gba/gba_hw.c gba/gba_render.c gba/main_gba.c src/golf_core.c -T gba/gba.ld -o build/duffer_pro.elf -lgcc
	"$(ARM_OBJCOPY)" -O binary -S build/duffer_pro.elf build/duffer_pro.gba
	$(GBAFIX) build/duffer_pro.gba -p -t"DUFFER PRO" -cADFP -m01 -r0
	@echo Successfully built GBA ROM: build/duffer_pro.gba
else
	@mkdir -p build
	"$(ARM_GCC)" -std=c99 -O2 -mcpu=arm7tdmi -mthumb-interwork -marm -ffreestanding -fno-builtin -fno-common -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib -Isrc -Igba gba/crt0.s gba/gba_hw.c gba/gba_render.c gba/main_gba.c src/golf_core.c -T gba/gba.ld -o build/duffer_pro.elf -lgcc
	"$(ARM_OBJCOPY)" -O binary -S build/duffer_pro.elf build/duffer_pro.gba
	$(GBAFIX) build/duffer_pro.gba -p -t"DUFFER PRO" -cADFP -m01 -r0
	@echo Successfully built GBA ROM: build/duffer_pro.gba
endif

$(GBAFIX): tools/gbafix.c
ifeq ($(OS),Windows_NT)
	@if not exist build mkdir build
	$(CC) -std=c99 -O2 tools/gbafix.c -o $(GBAFIX)
else
	@mkdir -p build
	$(CC) -std=c99 -O2 tools/gbafix.c -o $(GBAFIX)
endif

macos: mac
mac:
ifeq ($(OS),Windows_NT)
	wsl -u root -d Debian -- bash /mnt/c/Users/adam/code/Duffer/tools/build_macos.sh
else
	bash tools/build_macos.sh
endif

.PHONY: all clean gba mac macos
