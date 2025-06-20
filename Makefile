#==========================================================#
#===============    Makefile v1.0    ======================#
#==========================================================#

#==========================================================
#  Commands
CC		= gcc
CPP		= g++

ifeq ($(DLL), 1)
MAKEFILE = Makefile_SO
else
MAKEFILE = Makefile_A
endif

OUTPUT = fxnet
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

ifeq ($(ASAN),1)
C_FLAGS		+= -fsanitize=address -fno-omit-frame-pointer -fuse-ld=gold
CXX_FLAGS	+= -fsanitize=address -fno-omit-frame-pointer -fuse-ld=gold
endif

ARFLAGS			= -rc
#==========================================================

#==========================================================
#  Commands
CODE_DIR = ./ 
INCLUDE_DIR =./ ./include
LIB_FILE = -L./lib -Lcrypto -lpthread
ifeq ($(DLL), 1)
LIB_FILE += -l:libfxnet.so
else
LIB_FILE += -lfxnet
endif

#  Flags
ifeq ($(ASAN),1)
LIB_FILE	+= -lasan
endif

ifeq ($(GPERF),1)
C_FLAGS		+= -DGPERF
CXX_FLAGS	+= -DGPERF
LIB_FILE += -ltcmalloc_and_profiler
endif

#==========================================================

OBJS = $(foreach i, $(CODE_DIR), $(shell find $(i) -maxdepth 1 -name "*.cpp"))
COBJS = $(foreach i, $(CODE_DIR), $(shell find $(i) -maxdepth 1 -name "*.cc"))
INCLUDE_FLAG = $(foreach i, $(INCLUDE_DIR), -I$(i))

COMMON = "common"
all:$(OUTPUT)

$(OUTPUT):$(OUTPUT_DIR) $(COMMON) $(COBJS:.cc=.o) $(OBJS:.cpp=.o)
	@echo Build...$@
	$(CPP) $(CXX_FLAGS) -o $(OUTPUT_DIR)/$(OUTPUT) $(addprefix $(OUTPUT_DIR)/,$(COBJS:.cc=.o)) $(addprefix $(OUTPUT_DIR)/,$(OBJS:.cpp=.o)) $(LIB_FILE)

%.o: %.cpp
	@echo Compile...$@
	$(CPP) $(CXX_FLAGS) $(INCLUDE_FLAG) -c $< -o $(OUTPUT_DIR)/$@ -MMD -MP -MF $(OUTPUT_DIR)/$(@:%.o=%.d) 

%.o: %.cc
	@echo Compile...$@
	$(CC) $(C_FLAGS) $(INCLUDE_FLAG) -c $< -o $(OUTPUT_DIR)/$@ -MMD -MP -MF $(OUTPUT_DIR)/$(@:%.o=%.d)

$(OUTPUT_DIR):
	-mkdir -p $(OUTPUT_DIR)

$(COMMON):
	@echo Compile...$@
	$(MAKE) -f $(MAKEFILE) -C $@

-include $(OBJS:.cpp=.d)
-include $(COBJS:.cc=.d)

clean:
	$(MAKE) -C common -f $(MAKEFILE) clean
	rm -f $(OUTPUT_DIR)/$(OUTPUT)
	rm -f $(addprefix $(OUTPUT_DIR)/,$(COBJS:.cc=.o))
	rm -f $(addprefix $(OUTPUT_DIR)/,$(COBJS:.cc=.d))
	rm -f $(addprefix $(OUTPUT_DIR)/,$(OBJS:.cpp=.o))
	rm -f $(addprefix $(OUTPUT_DIR)/,$(OBJS:.cpp=.d))
	
	@echo Clean ...done!
