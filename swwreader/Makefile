
#
#  Mac OS X / Linux / Windows (MSYS2) Makefile
#

UNAME := $(shell uname)
TOPDIR           =  ..

OPTIMIZATION     =  -O2 -g
OSGHOME          ?= /usr/local

OSG_INCLUDE      =  ${OSGHOME}/include
LOCAL_INCLUDE    =  $(TOPDIR)/include
NETCDF_INCLUDE   := $(shell nc-config --includedir 2>/dev/null || pkg-config --variable=includedir netcdf 2>/dev/null || echo /usr/include)

ifeq ($(UNAME), Darwin)
	# OS X
	OSX_LIBS        = -framework Carbon -framework OpenGL -lobjc
	LIB_TYPE	= -dynamiclib -F/System/Library/Frameworks
	TARGET           =  $(TOPDIR)/bin/lib$(NAME).dylib
	CFLAGS          = -DGL_SILENCE_DEPRECATION
else ifneq ($(findstring MINGW,$(UNAME)),)
	# Windows (MSYS2/MinGW)
	LIB_TYPE	= -shared
	CFLAGS		= -fPIC -fno-strict-aliasing -Wall
	TARGET           =  $(TOPDIR)/bin/lib$(NAME).dll
	X_LIBS          =
else
	# Linux
	LIB_TYPE	= -shared
	CFLAGS		= -fPIC -fno-strict-aliasing -Wall
	TARGET           =  $(TOPDIR)/bin/lib$(NAME).so
	TARGET_FINAL_DIR = /usr/local/lib/lib$(NAME).so.1
endif

# GDAL is omitted by default — tile textures use a .georef sidecar instead.
# To build with GDAL (enables geotransform reading from user-supplied GeoTIFFs):
#   make GDAL_LIBS="$(gdal-config --libs)" GDAL_CFLAGS="$(gdal-config --cflags)"
GDAL_LIBS   ?=
GDAL_CFLAGS ?=
ifneq ($(GDAL_LIBS),)
CFLAGS += -DHAVE_GDAL
endif
GDAL_INCLUDE = $(GDAL_CFLAGS)

INCLUDES         =  -I $(LOCAL_INCLUDE) -I $(NETCDF_INCLUDE) -I $(OSG_INCLUDE) $(GDAL_INCLUDE)
NETCDF_LIBS      =  $(shell nc-config --libs 2>/dev/null || pkg-config --libs netcdf 2>/dev/null || echo -lnetcdf)
X_LIBS           ?= -lX11
OTHER_LIBS       =  -lm -lstdc++
LIBS            +=  -losg -losgDB -lOpenThreads $(NETCDF_LIBS) $(X_LIBS) $(OTHER_LIBS) $(OSX_LIBS) $(GDAL_LIBS)
LIBDIRS          =  -L${OSGHOME}/lib -L/usr/lib -L/usr/X11R6/lib -L/usr/local/lib64 -L/usr/local/lib

COMPILER         =  g++
NAME             =  swwreader
OBJ              =  filechangedcheck.o swwreader.o


$(TARGET) : $(OBJ)
	$(COMPILER) $(OPTIMIZATION) $(LIB_TYPE) $(LIBDIRS) $(OBJ) $(LIBS) -o $(TARGET)

-include $(OBJ:.o=.d)

%.o : %.cpp
	$(COMPILER) $(OPTIMIZATION) $(CFLAGS) -MMD -MP -c $(INCLUDES) $< -o $@


clean :
	rm -f *.o *.d *~ $(TARGET)

install :
	# Linux - create symbolic link for version-independent access
	cp $(TARGET) $(TARGET_FINAL_DIR)
	ln -sf $(TARGET_FINAL_DIR) /usr/local/lib/lib$(NAME).so
	ldconfig


