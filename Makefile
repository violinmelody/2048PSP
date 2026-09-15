TARGET := 2048
OBJS := src/main.o src/app.o src/game.o src/renderer.o src/storage.o src/audio.o

INCDIR := include
CFLAGS := -O2 -G0 -Wall -Wextra -Wshadow -Wstrict-prototypes -ffast-math
CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS := $(CFLAGS)
LIBS := -lpspgu -lpspaudio -lpsprtc -lm

EXTRA_TARGETS := EBOOT.PBP
PSP_EBOOT_TITLE := 2048
SFOFLAGS += -s APP_VER=1.00
PSP_EBOOT_ICON := assets/ICON0.PNG

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
