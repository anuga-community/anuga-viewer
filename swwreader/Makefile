
#
#  Mac OS X / Linux / Windows (MSYS2) Makefile
#

UNAME := $(shell uname)
TOPDIR           =  ..

OPTIMIZATION     =  -O2
OSGHOME          ?= /usr/local

OSG_INCLUDE      =  ${OSGHOME}/include
LOCAL_INCLUDE    =  $(TOPDIR)/include
GDAL_INCLUDE     =  `gdal-config --cflags`
NETCDF_INCLUDE   := $(shell nc-config --includedir 2>/dev/null || pkg-config --variable=includedir netcdf 2>/dev/null || echo /usr/include)
INCLUDES         =  -I $(LOCAL_INCLUDE) -I $(NETCDF_INCLUDE) -I $(OSG_INCLUDE) $(GDAL_INCLUDE)

ifeq ($(UNAME), Darwin)
	# OS X
	OSX_LIBS        = -framework Carbon -framework OpenGL -lobjc
	LIB_TYPE	= -dynamiclib -F/System/Library/Frameworks
	TARGET           =  $(TOPDIR)/bin/lib$(NAME).dylib
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

NETCDF_LIBS      =  $(shell nc-config --libs 2>/dev/null || pkg-config --libs netcdf 2>/dev/null || echo -lnetcdf)
X_LIBS           ?= -lX11
GDAL_LIBS        =  `gdal-config --libs`
OTHER_LIBS       =  -lm -lstdc++
LIBS            +=  -losg -losgDB -lOpenThreads $(NETCDF_LIBS) $(X_LIBS) $(OTHER_LIBS) $(OSX_LIBS) $(GDAL_LIBS)
LIBDIRS          =  -L${OSGHOME}/lib -L/usr/lib -L/usr/X11R6/lib -L/usr/local/lib64 -L/usr/local/lib

COMPILER         =  g++
NAME             =  swwreader
OBJ              =  filechangedcheck.o swwreader.o


$(TARGET) : $(OBJ)
	$(COMPILER) $(OPTIMIZATION) $(LIB_TYPE) $(LIBDIRS) $(LIBS) $(OBJ) -o $(TARGET)

%.o : %.cpp
	$(COMPILER) $(OPTIMIZATION) $(CFLAGS) -c $(INCLUDES) $< -o $@


clean :
	rm -f *.o *~ $(TARGET)

install :
	# Linux - create symbolic link for version-independent access
	cp $(TARGET) $(TARGET_FINAL_DIR)
	ln -sf $(TARGET_FINAL_DIR) /usr/local/lib/lib$(NAME).so
	ldconfig


