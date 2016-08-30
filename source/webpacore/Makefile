BUILD_PLATFORM := PLATFORM_RDKB
#BUILD_PLATFORM := PLATFORM_RDKV

WEBPA_ROOT_DIR  := ./../..
BUILD_ROOT_DIR  := $(WEBPA_ROOT_DIR)/..

ifeq ($(BUILD_PLATFORM), PLATFORM_RDKB)
WEBPA_OUT_DIR   := $(BUILD_ROOT_DIR)/Out/intel_usg-rdkb_arm/usr/ccsp/
WEBPA_STAGE_DIR := $(BUILD_ROOT_DIR)/Stage/intel_usg-rdkb_arm/lib
endif

# WebPA Includes
INCPATH += $(WEBPA_ROOT_DIR)/include
INCPATH += $(WEBPA_ROOT_DIR)/source/webpacore/include

# Opensource Includes for RDKB
ifeq ($(BUILD_PLATFORM), PLATFORM_RDKB)
INCPATH += $(BUILD_ROOT_DIR)/ExtDependency/opensource_work/intel_usg/rdkb_arm/nopoll/src
LDFLAGS += -L$(WEBPA_OUT_DIR)/../../lib/ -lnopoll
INCPATH += $(BUILD_ROOT_DIR)/ExtDependency/opensource_work/intel_usg/rdkb_arm/cjson
LDFLAGS += -L$(WEBPA_OUT_DIR)/../../lib/ -lcjson
INCPATH += $(BUILD_ROOT_DIR)/ExtDependency/opensource_work/intel_usg/rdkb_arm/msgpack/include
LDFLAGS += -L$(WEBPA_OUT_DIR)/../../lib/ -lmsgpack
CFLAGS += "-DPLATFORM_RDKB"
endif

# Cross Compiler Path for RDKB
ifeq ($(BUILD_PLATFORM), PLATFORM_RDKB)
CC = $(WEBPA_ROOT_DIR)/../../sdk/arm/toolchain/staging_dir/usr/bin/armeb-linux-gcc
endif

CFLAGS += $(addprefix -I, $(INCPATH))
CFLAGS += -fPIC -Wall 
LDFLAGS += -shared 

RM = rm -f  
TARGET_LIB = libwebpacore.so 
SRCS = wrp_mgr.c websocket_mgr.c
OBJS = $(SRCS:.c=.o)

all: ${TARGET_LIB} install

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

install:
# Installing libraries 
	@echo "Installing webpacore libraries"
	@install -d -m 0755 $(WEBPA_OUT_DIR)/webpa/
	@install -m 0755 $(TARGET_LIB) $(WEBPA_OUT_DIR)/webpa/	
	@cp -f $(TARGET_LIB) $(WEBPA_OUT_DIR)
	@cp $(TARGET_LIB) $(WEBPA_STAGE_DIR)

clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)
	
.PHONY: all clean
