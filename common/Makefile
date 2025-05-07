#==========================================================#
#===============    Makefile v1.0    ======================#
#==========================================================#

#==========================================================
#  Commands
CC		= gcc
CPP		= g++
AR		= ar
RANLIB	= ranlib

OUTPUT = libfxnet.a

ifeq ($(shell uname -m), x86_64)
MOUTPUT_DIR = ./x64
else
MOUTPUT_DIR = ./
endif

#  Flags
ifeq ($(BUILD),DEBUG)
D = d
C_FLAGS			= -g -O0 -rdynamic -Wall -D_DEBUG -DLINUX -pg
CXX_FLAGS 		= -g -O0 -rdynamic -Wall -Woverloaded-virtual -D_DEBUG -DLINUX -pg
OUTPUT_DIR      = $(MOUTPUT_DIR)/DEBUG
else
D = 
C_FLAGS			= -g -rdynamic -Wall -DNDEBUG -DLINUX
CXX_FLAGS 		= -g -rdynamic -Wall -Woverloaded-virtual -DNDEBUG -DLINUX
OUTPUT_DIR      = $(MOUTPUT_DIR)/RELEASE
endif

ifeq ($(SINGLE_THREAD),1)
C_FLAGS			+= -D__SINGLE_THREAD__
CXX_FLAGS		+= -D__SINGLE_THREAD__
endif

ARFLAGS			= -rc
#==========================================================

#==========================================================
#  Commands
CODE_DIR    = ./
INCLUDE_DIR = ../include/

#==========================================================

OBJS = $(foreach i, $(CODE_DIR), $(shell find $(i) -name "*.cpp"))
COBJS = $(foreach i, $(CODE_DIR), $(shell find $(i) -name "*.cc"))
INCLUDE_FLAG = $(foreach i, $(INCLUDE_DIR), -I$(i))

all:$(OUTPUT)

$(OUTPUT):$(COBJS:.cc=.o) $(OBJS:.cpp=.o) $(OUTPUT_DIR)
	@echo Build...$@
	$(AR) $(ARFLAGS) $(OUTPUT_DIR)/$(OUTPUT) $(COBJS:.cc=.o) $(OBJS:.cpp=.o)
	@$(RANLIB) $(OUTPUT_DIR)/$(OUTPUT)
	cp $(OUTPUT_DIR)/$(OUTPUT) ../lib

%.o: %.cpp
	@echo Compile...$@
	$(CPP) $(CXX_FLAGS) $(INCLUDE_FLAG) -c $< -o $@ -MMD -MP -MF$(@:%.o=%.d) 

%.o: %.cc
	@echo Compile...$@
	$(CC) $(C_FLAGS) $(INCLUDE_FLAG) -c $< -o $@ -MMD -MP -MF$(@:%.o=%.d)

$(OUTPUT_DIR):
	-mkdir -p $(OUTPUT_DIR)
	
-include $(OBJS:.cpp=.d)
-include $(COBJS:.cc=.d)

clean:
	rm -f $(OUTPUT_DIR)/$(OUTPUT)
	rm -f $(OBJS:.cpp=.o)
	rm -f $(OBJS:.cpp=.d)
	rm -f $(COBJS:.cc=.o)
	rm -f $(COBJS:.cc=.d)
	
	@echo Clean ...done!
