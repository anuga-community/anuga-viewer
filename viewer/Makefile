
#
#  Mac OS X / Linux / Windows (MSYS2) Makefile
#

UNAME := $(shell uname)
TOPDIR           =  ..

OPTIMIZATION     =  -O2 -g
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
else ifneq ($(findstring MINGW,$(UNAME)),)
	# Windows (MSYS2/MinGW)
	CPPFLAGS         =  -Wall $(OPTIMIZATION)
	X_LIBS          =
	WIN_LDFLAGS      =  -Wl,-subsystem,console
else
	# Linux
	CPPFLAGS         =  -Wall $(OPTIMIZATION)
endif

# GDAL omitted by default — tile textures use libcurl + .georef sidecar instead.
# Build with GDAL (enables geotransform reading from user-supplied GeoTIFFs):
#   make GDAL_LIBS="$(gdal-config --libs)" GDAL_CFLAGS="$(gdal-config --cflags)"
GDAL_LIBS   ?=
GDAL_CFLAGS ?=
ifneq ($(GDAL_LIBS),)
	CPPFLAGS        += -DHAVE_GDAL
endif

# libcurl for map tile fetching (replaces GDAL WMS on all platforms)
CURL_LIBS   = $(shell pkg-config --libs libcurl 2>/dev/null || curl-config --libs 2>/dev/null || echo "-lcurl")
CURL_CFLAGS = $(shell pkg-config --cflags libcurl 2>/dev/null || curl-config --cflags 2>/dev/null)
ifneq ($(CURL_LIBS),)
	CPPFLAGS    += -DHAVE_CURL
endif

X_LIBS           ?= -lX11
OTHER_LIBS       =  -lm -lstdc++ -lnetcdf
LIBS            +=  -losg -losgText -losgDB -losgUtil -losgGA \
                    -lOpenThreads -losgViewer -lswwreader \
                    $(GL_LIBS) $(X_LIBS) $(OTHER_LIBS) $(OSX_LIBS) $(GDAL_LIBS) $(CURL_LIBS)
# Put $(TOPDIR)/bin first so the freshly built swwreader is preferred over any installed version.
# Embed $ORIGIN rpath so the binary finds libswwreader.so next to itself at runtime (no sudo install needed).
LIBDIRS          =  -L$(TOPDIR)/bin -L${OSGHOME}/lib -L/usr/lib -L/usr/X11R6/lib -L/usr/local/lib64 -L/usr/local/lib
RPATH_FLAGS      = -Wl,-rpath,\$$ORIGIN -Wl,-rpath,/usr/local/lib

NAME             =  anuga_viewer
TARGETDIR        =  $(TOPDIR)/bin
TARGET           =  $(TARGETDIR)/$(NAME)

COMPILER         =  g++
OBJ              =  anugahud.o hud.o keyboardeventhandler.o watersurface.o main.o version.o \
                    bedslope.o skybox.o linegraph.o customviewer.o osmtexture.o \
                    directionallight.o state.o meshobject.o customargumentparser.o


%.o : %.cpp
	$(COMPILER) -c -MMD -MP $(INCLUDES) $(GDAL_CFLAGS) $(CURL_CFLAGS) $(CPPFLAGS) $< -o $@


$(TARGET) : $(OBJ)
	$(COMPILER) $(CPPFLAGS) $(LIBDIRS) $(RPATH_FLAGS) $(OBJ) $(LIBS) $(WIN_LDFLAGS) -o $(TARGET)

-include $(OBJ:.o=.d)


.PHONY : version.cpp

# on every build, recreate version() string based on SVN revision number
version.cpp:
	printf 'const char* version() { const char* s = "Revision: ' > version.cpp
	git rev-list --count HEAD | tr '\n' ' ' >> version.cpp
	printf '"; return s; }\n' >> version.cpp


clean :
	rm -f *.o *.d *~ $(TARGET)

