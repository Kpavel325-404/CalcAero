TARGET = calcaero
OBJS = src/main_psp.o src/platform_psp.o src/calc.o src/tools.o src/app.o src/store.o src/ui.o src/gfx.o src/gfx_psp.o src/font.o src/selftest.o src/aero.o src/symbolic.o src/dimensions.o src/aero_state.o src/analyze.o src/mathview.o src/aero_export.o src/l10n.o src/config.o src/unicode_font.o
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -std=gnu11 -I$(PSPDEV)/psp/include/freetype2 -I$(PSPDEV)/psp/include/harfbuzz
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lpspgu -lpsputility -lpsppower -lpspctrl -lharfbuzz -lfreetype -lpng -lz -lbz2 -lstdc++ -lm
BUILD_PRX = 1
PSP_FW_VERSION = 660
PSP_LARGE_MEMORY = 0
USE_KERNEL_LIBS = 0
SFOFLAGS = -s PSP_SYSTEM_VER=6.60
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = CalcAero 1.3
PSP_EBOOT_ICON = ICON0.png
PSP_EBOOT_PIC1 = PIC1.png
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
PARAM.SFO: Makefile
EBOOT.PBP: ICON0.png PIC1.png
$(OBJS): src/calc.h src/app.h src/tools.h src/store.h src/platform.h src/gfx.h src/font.h src/selftest.h
$(OBJS): src/aero.h src/mathview.h
src/ui.o: src/ui_aero.inc

$(OBJS): src/l10n.h src/unicode_font.h
EBOOT-SIGNED.PBP: EBOOT.PBP
	ebootsign $< $@
