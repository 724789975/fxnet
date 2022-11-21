#define macros

CC = cl
CFLAGS = /c /analyze- /W3 /Zc:wchar_t /Gm- /Zc:inline /fp:precise /D"WIN32" /D"_LIB" /D"_CRT_SECURE_NO_DEPRECATE" /D"_CRT_NONSTDC_NO_DEPRECATE" /D"_MBCS" /errorReport:prompt /WX- /Zc:forScope /Gd /Oy- /FC /EHsc /nologo /diagnostics:classic

!IFDEF WIN32
PLATFORM_DIR = ""
!ELSE
PLATFORM_DIR = x64\\
!ENDIF

TARGET = NetLib

!IF "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS) /D"_DEBUG" /JMC /GS /ZI /Od /RTC1 /MTd
DIR_OUT = ..\\$(PLATFORM_DIR)DEBUG\\
OBJ_OUT = .\\$(PLATFORM_DIR)DEBUG
!ELSE
CFLAGS = $(CFLAGS) /D"NDEBUG" /GS /GL /Gy /Zi /Oi /O2 /MT
DIR_OUT = ..\\$(PLATFORM_DIR)RELEASE\\
OBJ_OUT = .\\$(PLATFORM_DIR)RELEASE
!ENDIF

CFLAGS = $(CFLAGS) /Fd"$(OBJ_OUT)\$(TARGET)_nmake.pdb" /Fp"$(DIR_OUT)\$(TARGET).pch"

EXECUTABLE_NAME = $(TARGET).exe
STATIC_LIB_NAME = $(TARGET).lib
DIR_SRC = .\\
DIR_INCLUDE = \
		/I "../include/"
        
LK = link
LKFLAGS = /lib /NOLOGO

!IF "$(DEBUG)" == "0"
LKFLAGS = $(LKFLAGS) /OPT:REF /OPT:ICF
!ELSE
LKFLAGS = $(LKFLAGS)
!ENDIF

LKFLAGS = $(LKFLAGS) /PDB:"$(DIR_OUT)$(TARGET).lib.pdb" /ManifestFile:"$(OBJ_OUT)\$(STATIC_LIB_NAME).intermediate.manifest" /OUT:"$(DIR_OUT)$(STATIC_LIB_NAME)" /MANIFEST /NXCOMPAT /INCREMENTAL /SUBSYSTEM:WINDOWS /MANIFESTUAC:NO /TLBID:1

!IFNDEF WIN32
LKFLAGS = $(LKFLAGS) /MACHINE:X64
!ENDIF
 
# build application
target: $(STATIC_LIB_NAME)

# link objs
$(STATIC_LIB_NAME) : makeobj
	@echo Linking $(STATIC_LIB_NAME)...
	$(LK) $(LKFLAGS) $(OBJ_OUT)\*.obj

# cl ojbs
makeobj: $(OBJ_OUT)
	@for %%f in (*.cpp) do ( $(CC) $(CFLAGS) /Fo"$(OBJ_OUT)\%%~nf.obj" $(DIR_INCLUDE) %%f )
	@for %%f in (*.c) do ( $(CC) $(CFLAGS) /Fo"$(OBJ_OUT)\%%~nf.obj" $(DIR_INCLUDE) %%f )

$(OBJ_OUT):
	@if not exist $(OBJ_OUT) mkdir $(OBJ_OUT)

# delete output directories
clean:
	if exist $(OBJ_OUT) del $(OBJ_OUT)\*.obj
	if exist $(DIR_OUT) del $(DIR_OUT)\$(STATIC_LIB_NAME)
	if exist $(OBJ_OUT) del $(OBJ_OUT)\$(TARGET)_nmake.pdb

# create directories and build application
all: clean target
