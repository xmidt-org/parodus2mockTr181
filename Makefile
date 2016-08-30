
include $(CCSP_ROOT_DIR)/arch/ccsp_common.mk


modules := wal/wal
modules += webpacore
modules += wal/app

modules := $(addprefix $(ComponentSrcDir)/,$(modules))

all:
	@echo "Installing webpa modules"
	@for m in $(modules); do echo $$m; make -C $$m $@ || exit 1; done

clean:
	@for m in $(modules); do echo $$m; make -C $$m $@ || exit 1; done

install:
	@for m in $(modules); do echo $$m; make -C $$m $@ || exit 1; done

.PHONY: all clean install
