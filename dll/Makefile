#==========================================================#
#===============    Makefile v1.0    ======================#
#==========================================================#

#==========================================================
#  Commands
CC		= gcc
CPP		= g++

OUTPUT = libfxnet.so
ifeq ($(shell uname -m), x86_64)
MOUTPUT_DIR = ./x64
else
MOUTPUT_DIR = ./
endif

#  Flags
ifeq ($(DEBUG), 1)
D = d
C_FLAGS			= -g -O0 -rdynamic -Wall -D_DEBUG -DLINUX -fpic -ldl
CXX_FLAGS 		= -g -O0 -rdynamic -Wall -Woverloaded-virtual -D_DEBUG -DLINUX -fpic -ldl
OUTPUT_DIR      = $(MOUTPUT_DIR)/DEBUG
else
D = 
C_FLAGS			= -g -rdynamic -Wall -DNDEBUG -DLINUX -fpic -ldl
CXX_FLAGS 		= -g -rdynamic -Wall -Woverloaded-virtual -DNDEBUG -DLINUX -fpic -ldl
OUTPUT_DIR      = $(MOUTPUT_DIR)/RELEASE
endif

ifeq ($(SINGLE_THREAD),1)
C_FLAGS			+= -D__SINGLE_THREAD__
CXX_FLAGS		+= -D__SINGLE_THREAD__
endif

#==========================================================
#  Commands
CODE_DIR = ./ 
INCLUDE_DIR =./ ./include
LIB_FILE = -L../lib -Lcrypto -lpthread -lstdc++
LIB_FILE += -lfxnet

#==========================================================

OBJS = $(foreach i, $(CODE_DIR)src, $(shell find $(i) -maxdepth 1 -name "*.cpp"))
COBJS = $(foreach i, $(CODE_DIR)src, $(shell find $(i) -maxdepth 1 -name "*.cc"))
INCLUDE_FLAG = $(foreach i, $(INCLUDE_DIR), -I$(i))

all:$(OUTPUT)

$(OUTPUT):$(OUTPUT_DIR)_DIR $(COBJS:.cc=.o) $(OBJS:.cpp=.o)
	@echo Build...$@
	$(CC) -shared -o $(OUTPUT_DIR)/$(OUTPUT) $(addprefix $(OUTPUT_DIR)/,$(COBJS:.cc=.o)) $(addprefix $(OUTPUT_DIR)/,$(OBJS:.cpp=.o)) $(LIB_FILE)

$(OUTPUT_DIR)_DIR:
	@echo "mkdir"
	-mkdir -p $(OUTPUT_DIR)/src

%.o: %.cpp
	@echo Compile...$@
	$(CPP) $(CXX_FLAGS) $(INCLUDE_FLAG) -c $< -o $(OUTPUT_DIR)/$@ -MMD -MP -MF $(OUTPUT_DIR)/$(@:%.o=%.d) 

%.o: %.cc
	@echo Compile...$@
	$(CC) $(C_FLAGS) $(INCLUDE_FLAG) -c $< -o $(OUTPUT_DIR)/$@ -MMD -MP -MF $(OUTPUT_DIR)/$(@:%.o=%.d)

-include $(OBJS:.cpp=.d)
-include $(COBJS:.cc=.d)

clean:
	rm -f $(OUTPUT_DIR)/$(OUTPUT)
	rm -f $(addprefix $(OUTPUT_DIR)/,$(COBJS:.cc=.o))
	rm -f $(addprefix $(OUTPUT_DIR)/,$(COBJS:.cc=.d))
	rm -f $(addprefix $(OUTPUT_DIR)/,$(OBJS:.cpp=.o))
	rm -f $(addprefix $(OUTPUT_DIR)/,$(OBJS:.cpp=.d))
	
	@echo Clean ...done!
