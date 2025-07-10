common = "common"
dll = "dll"

all: $(common) $(dll)
	@echo Building completed

$(common):
	$(MAKE) -C $(common) -f Makefile_A $0

$(dll):
	$(MAKE) -C $(dll) $0

clean:
	$(MAKE) -C $(common) -f Makefile_A clean
	$(MAKE) -C $(dll) clean
