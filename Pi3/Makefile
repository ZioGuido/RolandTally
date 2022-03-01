PROGRAMS = RolandTally
RM = /bin/rm
SRC_PATH = ../
INCLUDE = ../
OBJECT_PATH = .
vpath %.o $(OBJECT_PATH)

OBJECTS	 =	RtMidi.o

CC       = g++ #-g
DEFS     = -D__LINUX_ALSA__ -DAVOID_TIMESTAMPING

CFLAGS   = -std=c++11 #-O2
CFLAGS	+= -march=armv8-a+crc -mfpu=neon-vfpv4 -mtune=cortex-a53 -ftree-vectorize -mfloat-abi=hard
CFLAGS  += -I$(INCLUDE)

LIBRARY  = -lpthread -lasound 
#LIBRARY   += -lbcm2835
LIBRARY   += -lwiringPi


%.o : $(SRC_PATH)/%.cpp
	$(CC) $(CFLAGS) $(DEFS) -c $(<) -o $(OBJECT_PATH)/$@

%.o : $(SRC_PATH)/RtMidi/%.cpp
	$(CC) $(CFLAGS) $(DEFS) -c $(<) -o $(OBJECT_PATH)/$@

all : $(PROGRAMS)

RolandTally : ../main.cpp $(OBJECTS)
	$(CC) $(CFLAGS) $(DEFS) -o RolandTally ../main.cpp $(OBJECT_PATH)/*.o $(LIBRARY)

clean : 
	$(RM) -f $(OBJECT_PATH)/*.o
	$(RM) -f $(PROGRAMS)
	$(RM) -f *.raw *~ *.exe
	$(RM) -fR *.dSYM
