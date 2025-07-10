common = "builld_common"
dll = "build_dll"
MAKEFILE = Makefile.mak

all: $(common) $(dll)
	@echo Building completed

$(common):
	cd common
	$(MAKE) /f $(MAKEFILE) $0
	cd ..
	xcopy x64 dll\x64 /y /i /e

$(dll):
	cd dll
	$(MAKE) /f $(MAKEFILE) $0
	cd ..

clean:
	cd common
	$(MAKE) /f $(MAKEFILE) clean
	cd ..
	cd dll
	$(MAKE) /f $(MAKEFILE) clean
	cd ..
