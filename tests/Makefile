
#
#  Mac OS X / Linux / Windows (MSYS2) Makefile
#

UNAME := $(shell uname)

OPTIMIZATION     =  -O2
OSGHOME          ?= /usr/local

OSG_INCLUDE      =  ${OSGHOME}/include
LOCAL_INCLUDE    =  ../include
GDAL_INCLUDE     =  `gdal-config --cflags`
NETCDF_INCLUDE   := $(shell nc-config --includedir 2>/dev/null || pkg-config --variable=includedir netcdf 2>/dev/null || echo /usr/include)
INCLUDES         =  -I $(LOCAL_INCLUDE) -I $(NETCDF_INCLUDE) -I $(OSG_INCLUDE) $(GDAL_INCLUDE)

ifeq ($(UNAME), Darwin)
	# OS X
	OSX_LIBS        = -framework Carbon -framework OpenGL -lobjc
	TARGET           =  tests
	CFLAGS          = -DGL_SILENCE_DEPRECATION
else ifneq ($(findstring MINGW,$(UNAME)),)
	# Windows (MSYS2/MinGW)
	TARGET           =  tests
	X_LIBS          =
else
	# Linux
	CFLAGS		=  -Wall
	TARGET           =  tests
endif

NETCDF_LIBS      =  $(shell nc-config --libs 2>/dev/null || pkg-config --libs netcdf 2>/dev/null || echo -lnetcdf)
X_LIBS           ?= -lX11
GDAL_LIBS        =  `gdal-config --libs`
OTHER_LIBS       =  -lm -lstdc++
LIBS            +=  -losg -losgDB -lOpenThreads $(NETCDF_LIBS) $(X_LIBS) $(OTHER_LIBS) $(OSX_LIBS) $(GDAL_LIBS) -lcppunit
LIBDIRS          =  -L${OSGHOME}/lib -L/usr/lib -L/usr/X11R6/lib -L/usr/local/lib -L../bin
TESTLIBS	= -lswwreader

COMPILER         =  g++
NAME             =  swwreader
OBJ              =  touchedfiletest.o swwreadertest.o


$(TARGET) : $(OBJ)
	$(COMPILER) $(OPTIMIZATION) $(LIBDIRS) $(OBJ) $(TESTLIBS) $(LIBS) -o $(TARGET)

%.o : %.cpp
	$(COMPILER) $(OPTIMIZATION) $(CFLAGS) -c $(INCLUDES) $< -o $@


clean :
	rm -f *.o *~ $(TARGET)

test : clean $(TARGET)
	./tests
