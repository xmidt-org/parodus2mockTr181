include $(CCSP_ROOT_DIR)/arch/ccsp_common.mk

INCPATH += $(CCSP_ROOT_DIR)/webpa/include
INCPATH += $(CCSP_ROOT_DIR)/webpa/source/webpacore/include
CFLAGS += $(addprefix -I, $(INCPATH))

CFLAGS += -Wall
CFLAGS += "-DPLATFORM_RDKB"
INCPATH += $(CCSP_ROOT_DIR)/ExtDependency/opensource_work/intel_usg/rdkb_arm/nopoll/src

LDFLAGS    += -L$(CCSP_OUT_DIR)/webpa/ -lwal -Wl,-unresolved-symbols=ignore-in-shared-libs -lm
LDFLAGS    += -L$(CCSP_ROOT_DIR)/webpa/source/webpacore/ -lwebpacore -Wl,-unresolved-symbols=ignore-in-shared-libs

target := $(ComponentBuildDir)/webpa

source_files := $(call add_files_from_base,,'*.c')
obj_files := $(addprefix $(ComponentBuildDir)/, $(source_files:%.c=%.o))
-include $(obj_files:.o=.d)

$(target): $(obj_files)

#
#	Build targets
#
all: clean $(target) install

.PHONY: all clean

clean:
	@echo "Cleaning WebPA wal/app"
	rm -Rf $(ComponentBuildDir)

install:
	@echo "Installing webpa Installables"
	@install -d -m 0755 $(CCSP_OUT_DIR)/webpa
	@install -m 0755 $(target) $(CCSP_OUT_DIR)/webpa
	@cp -f $(CCSP_ROOT_DIR)/webpa/config/webpa_cfg.json $(CCSP_OUT_DIR)/webpa


#
# include custom post makefile, if exists
#

ifneq ($(findstring $(CCSP_CMPNT_BUILD_CUSTOM_MK_POST), $(wildcard $(ComponentBoardDir)/*.mk)), )
    include $(ComponentBoardDir)/$(CCSP_CMPNT_BUILD_CUSTOM_MK_POST)
endif

