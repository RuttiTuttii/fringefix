CC ?= cc
CC_WIN ?= x86_64-w64-mingw32-gcc
WINDRES ?= x86_64-w64-mingw32-windres
# версия берётся из git describe; сборка не из тега считается dev-сборкой, у неё обновления выключены
VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)
# owner/name репозитория, откуда качаются релизы; пусто значит обновления отключены
REPO ?=

# WERROR=1 делает предупреждения ошибками, SAN=1 собирает тесты под ASan и UBSan
SAN_ON := -fsanitize=address,undefined -fno-omit-frame-pointer -g
WARN = -Wall -Wextra -Wpedantic $(if $(WERROR),-Werror)
INC = -Iinclude

PORTABLE = src/config.c src/release.c src/sha256.c src/profile.c src/i18n.c src/layout.c src/applist.c
WIN_SRC = src/main.c src/app.c src/capture.c src/gfx.c src/gfx_frame.c src/gfx_sig.c src/shader.c \
	src/output.c src/pattern.c src/ui_tray.c src/ui_settings.c src/ui_controls.c src/ui_paint.c src/ui_profile.c \
	src/ui_update.c src/ui_wizard.c src/ui_versions.c src/gamedetect.c src/slider.c src/theme.c src/profile_io.c src/update.c src/update_install.c src/http.c $(PORTABLE)
WIN_LIBS = -ld3d11 -ldxgi -ld3dcompiler -ldxguid -luuid -luser32 -lgdi32 -lcomctl32 \
	-lshell32 -lwinhttp -lcomdlg32 -ldwmapi -luxtheme -ladvapi32 -lpsapi
WIN_FLAGS = -std=gnu99 -O2 $(WARN) -fvisibility=hidden $(INC) -D_WIN32_WINNT=0x0A00 \
	-DFF_VERSION='"$(VERSION)"' -DFF_REPO='"$(REPO)"'

TESTS = config release sha256 profile i18n layout applist

# собирается под Windows кросс-компилятором mingw-w64 или нативно в msys2 (CC_WIN=gcc WINDRES=windres)
win: build/fringefix.exe

build/fringefix.exe: $(WIN_SRC) $(wildcard include/*.h) $(wildcard src/res/*)
	mkdir -p build
	$(WINDRES) -O coff -i src/res/fringefix.rc -o build/fringefix_rc.o
	$(CC_WIN) $(WIN_FLAGS) -mwindows -o $@ $(WIN_SRC) build/fringefix_rc.o $(WIN_LIBS) -static

# модули без платформенного кода проверяются на любой системе
test: $(TESTS:%=build/test_%)
	for t in $(TESTS); do ./build/test_$$t || exit 1; done

build/test_%: tests/test_%.c src/%.c include/%.h tests/test.h
	mkdir -p build
	$(CC) -std=c99 -O2 $(WARN) $(if $(SAN),$(SAN_ON)) -fvisibility=hidden $(INC) -o $@ $(filter %.c,$^) -lm

# профиль собирается вместе с конфигом: оба нужны для проверки круга запись-чтение
build/test_profile: src/config.c

# значки генерируются скриптом и лежат в репозитории готовыми
icons:
	python3 -I tools/make_icons.py

clean:
	rm -rf build

.PHONY: win test icons clean
