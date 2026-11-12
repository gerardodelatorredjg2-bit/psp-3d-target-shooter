TARGET = psp3dshooter
OBJS = src/main.o
INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lpspgu -lpspgum -lpspge -lpspdisplay -lpspctrl -lpspkernel
BUILD_PRX = 0
PSP_FW_VERSION = 660
PSP_EBOOT_TITLE = PSP 3D Target Shooter
EXTRA_TARGETS = EBOOT.PBP

include $(PSPSDK)/lib/build.mak
