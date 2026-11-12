TARGET = psp3dshooter
OBJS = src/main.o
INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra -std=gnu99
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lpspaudio -lpspdebug -lpspgu -lpspgum -lpspge -lpspdisplay -lpspctrl -lpspkernel -lm
BUILD_PRX = 0
PSP_FW_VERSION = 660
PSP_EBOOT_TITLE = PSP Target Shooter 3D
EXTRA_TARGETS = EBOOT.PBP

PSPDEV ?= /opt/pspdev
PSPSDK ?= $(PSPDEV)/psp/sdk
include $(PSPSDK)/lib/build.mak

src/main.o: src/models_generated.h
