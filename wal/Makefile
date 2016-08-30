
include $(CCSP_ROOT_DIR)/arch/ccsp_common.mk

#
#	Set up include directories
#

INCPATH += $(CCSP_ROOT_DIR)/hal/include
INCPATH += $(CCSP_ROOT_DIR)/webpa/include
INCPATH += $(CCSP_ROOT_DIR)/webpa/source/wal/wal/include
INCPATH += $(CCSP_ROOT_DIR)/../generic/rdk_logger/include/
INCPATH += $(CCSP_ROOT_DIR)/ExtDependency/opensource_work/intel_usg/rdkb_arm/msgpack/include
INCPATH += $(CCSP_ROOT_DIR)/ExtDependency/opensource_work/intel_usg/rdkb_arm/base64

CFLAGS += $(addprefix -I, $(INCPATH))


CFLAGS += -Wall
CFLAGS += -DFEATURE_SUPPORT_RDKLOG
CFLAGS += "-DPLATFORM_RDKB"
LDFLAGS    += -L$(CCSP_OPENSOURCE_DIR)/lib -L$(CCSP_OUT_DIR)/../../lib/ -lrdkloggers -L$(CCSP_OUT_DIR)/../../lib/ -llog4c
LDFLAGS+= -lccsp_common -lm -lpthread -lbase64 -lmsgpack

target := $(ComponentBuildDir)/libwal.so
source_files := $(call add_files_from_base,,'*.c')
obj_files := $(addprefix $(ComponentBuildDir)/, $(source_files:%.c=%.o))
-include $(obj_files:.o=.d)
$(target): $(obj_files)

RPC_SOURCES := ../app/webpa_rpc_xdr.c ../app/webpa_rpc_clnt.c ../app/webpa_rpc_server_stubs.c
RPCGEN=rpcgen
RPCGENFLAGS=-MC -N

#
#	Build targets
#
all: rpcgenfiles $(target) install

.PHONY: all clean

clean:
	rm -Rf $(ComponentBuildDir)

rpcgenfiles:
	rm -Rf ../app/webpa_rpc.h $(RPC_SOURCES) ../app/$(ComponentBuildDir)
	$(RPCGEN) $(RPCGENFLAGS) -h ../app/webpa_rpc.x > ../app/webpa_rpc.h
	$(RPCGEN) $(RPCGENFLAGS) -m ../app/webpa_rpc.x > ../app/webpa_rpc_server_stubs.c
	$(RPCGEN) $(RPCGENFLAGS) -c ../app/webpa_rpc.x > ../app/webpa_rpc_xdr.c
	$(RPCGEN) $(RPCGENFLAGS) -l ../app/webpa_rpc.x > ../app/webpa_rpc_clnt.c

install:
	@echo "Installing wal Installables"
	@install -d -m 0755 $(CCSP_OUT_DIR)/webpa
	@install -m 0755 $(target) $(CCSP_OUT_DIR)/webpa
	@cp -f $(target) $(CCSP_OUT_DIR)/
	@cp -f $(CCSP_ROOT_DIR)/webpa/source/wal/wal/arch/intel_usg/boards/rdkb_arm/config/comcast/WebpaAgent.xml $(CCSP_OUT_DIR)/webpa
	@cp $(target) $(CCSP_STAGING_ROOT)/lib


#
# include custom post makefile, if exists
#
ifneq ($(findstring $(CCSP_CMPNT_BUILD_CUSTOM_MK_POST), $(wildcard $(ComponentBoardDir)/*.mk)), )
    include $(ComponentBoardDir)/$(CCSP_CMPNT_BUILD_CUSTOM_MK_POST)
endif

