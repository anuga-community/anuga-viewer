
#
#  Mac OS X / Linux / Windows (MSYS2) Makefile
#

UNAME := $(shell uname)
TOPDIR           =  ..

OPTIMIZATION     =  -O2
OSGHOME          ?= /usr/local

OSG_INCLUDE      =  ${OSGHOME}/include
LOCAL_INCLUDE    =  $(TOPDIR)/include
INCLUDES         =  -I . -I $(LOCAL_INCLUDE) -I $(OSG_INCLUDE) -I/usr/X11R6/include

ifeq ($(UNAME), Darwin)
	# OS X
	OSX_LIBS        = -framework Carbon -framework OpenGL -lobjc
	MACOS_SDK       := $(shell xcrun --show-sdk-path 2>/dev/null)
	CPPFLAGS         =  -isysroot $(MACOS_SDK) \
		            -DGL_SILENCE_DEPRECATION \
		            -Wall -DDARWIN_QUICKTIME \
		            $(OPTIMIZATION)
	GDAL_LIBS        = $(shell gdal-config --libs 2>/dev/null)
else ifneq ($(findstring MINGW,$(UNAME)),)
	# Windows (MSYS2/MinGW)
	CPPFLAGS         =  -Wall $(OPTIMIZATION)
	X_LIBS          =
	WIN_LDFLAGS      =  -Wl,-subsystem,console
	GDAL_LIBS        =
else
	# Linux
	CPPFLAGS         =  -Wall $(OPTIMIZATION)
	GDAL_LIBS        = $(shell gdal-config --libs 2>/dev/null)
endif

X_LIBS           ?= -lX11
OTHER_LIBS       =  -lm -lstdc++ -lnetcdf
LIBS            +=  -losg -losgText -losgDB -losgUtil -losgGA \
                    -lOpenThreads -losgViewer -lswwreader \
                    $(GL_LIBS) $(X_LIBS) $(OTHER_LIBS) $(OSX_LIBS) $(GDAL_LIBS)
# Put $(TOPDIR)/bin first so the freshly built swwreader is preferred over any installed version
LIBDIRS          =  -L$(TOPDIR)/bin -L${OSGHOME}/lib -L/usr/lib -L/usr/X11R6/lib -L/usr/local/lib64 -L/usr/local/lib

NAME             =  anuga_viewer
TARGETDIR        =  $(TOPDIR)/bin
TARGET           =  $(TARGETDIR)/$(NAME)

COMPILER         =  g++
OBJ              =  anugahud.o hud.o keyboardeventhandler.o watersurface.o main.o version.o \
                    bedslope.o skybox.o linegraph.o customviewer.o \
                    directionallight.o state.o meshobject.o customargumentparser.o



%.o : %.cpp
	$(COMPILER) -c $(INCLUDES) $(CPPFLAGS) $< -o $@


$(TARGET) : $(OBJ)
	$(COMPILER) $(CPPFLAGS) $(LIBDIRS) $(OBJ) $(LIBS) $(WIN_LDFLAGS) -o $(TARGET)


.PHONY : version.cpp

# on every build, recreate version() string based on SVN revision number
version.cpp:
	printf 'const char* version() { const char* s = "Revision: ' > version.cpp
	git rev-list --count HEAD | tr '\n' ' ' >> version.cpp
	printf '"; return s; }\n' >> version.cpp


clean :
	rm -f *.o *~ $(TARGET)

