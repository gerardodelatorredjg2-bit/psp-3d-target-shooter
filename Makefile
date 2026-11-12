TARGET = psp3dshooter
OBJS = src/main.o
INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lpspdebug -lpspgu -lpspgum -lpspge -lpspdisplay -lpspctrl -lpspkernel -lm
BUILD_PRX = 0
PSP_FW_VERSION = 660
PSP_EBOOT_TITLE = PSP 3D Target Shooter
EXTRA_TARGETS = EBOOT.PBP

# The PSPDEV container exports PSPDEV but may not export PSPSDK.
PSPSDK ?= $(PSPDEV)/psp/sdk
include $(PSPSDK)/lib/build.mak
