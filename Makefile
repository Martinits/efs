######## SGX SDK Settings ########

SGX_SDK ?= /opt/intel/sgxsdk
SGX_SSL ?= /opt/intel/sgxssl
SGX_MODE ?= SIM
SGX_ARCH ?= x64
SGX_DEBUG ?= 1

include $(SGX_SDK)/buildenv.mk

ifeq ($(shell getconf LONG_BIT), 32)
	SGX_ARCH := x86
else ifeq ($(findstring -m32, $(CFLAGS)), -m32)
	SGX_ARCH := x86
endif

ifeq ($(SGX_ARCH), x86)
	SGX_COMMON_FLAGS := -m32
	SGX_LIB_PATH := $(SGX_SDK)/lib
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x86/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x86/sgx_edger8r
else
	SGX_COMMON_FLAGS := -m64
	SGX_LIB_PATH := $(SGX_SDK)/lib64
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r
endif

ifeq ($(SGX_DEBUG), 1)
ifeq ($(SGX_PRERELEASE), 1)
$(error Cannot set SGX_DEBUG and SGX_PRERELEASE at the same time!!)
endif
endif

ifeq ($(SGX_DEBUG), 1)
        SGX_COMMON_FLAGS += -O0 -g
else
        SGX_COMMON_FLAGS += -O2
endif

SGX_COMMON_FLAGS += -Wall -Wextra -Winit-self -Wpointer-arith -Wreturn-type \
                    -Waddress -Wsequence-point -Wformat-security \
                    -Wmissing-include-dirs -Wfloat-equal -Wundef -Wshadow \
                    -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls
SGX_COMMON_CFLAGS := $(SGX_COMMON_FLAGS) -Wjump-misses-init -Wstrict-prototypes \
		     -Wunsuffixed-float-constants -Wno-sign-conversion

######## App Settings ########

ifneq ($(SGX_MODE), HW)
	URTS_LIB_NAME := sgx_urts_sim
else
	URTS_LIB_NAME := sgx_urts
endif

APP_C_SOURCES := $(wildcard app/*.c)
APP_INCLUDE_PATHS := -Iinclude -Iapp -I$(SGX_SDK)/include

APP_CFLAGS := -fPIC -Wno-attributes $(APP_INCLUDE_PATHS) -DLOG_USE_COLOR

# Three configuration modes - Debug, prerelease, release
#   Debug - Macro DEBUG enabled.
#   Prerelease - Macro NDEBUG and EDEBUG enabled.
#   Release - Macro NDEBUG enabled.
ifeq ($(SGX_DEBUG), 1)
        APP_CFLAGS += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
        APP_CFLAGS += -DNDEBUG -DEDEBUG -UDEBUG
else
        APP_CFLAGS += -DNDEBUG -UEDEBUG -UDEBUG
endif

APP_LINK_FLAGS := -L$(SGX_LIB_PATH) -l$(URTS_LIB_NAME)

APP_C_OBJS := $(APP_C_SOURCES:.c=.o)

APP_NAME := efs

######## Enclave Settings ########

ifneq ($(SGX_MODE), HW)
	TRTS_LIB_NAME := sgx_trts_sim
	SERVICE_LIB_NAME := sgx_tservice_sim
else
	TRTS_LIB_NAME := sgx_trts
	SERVICE_LIB_NAME := sgx_tservice
endif
CRYPTO_LIB_NAME := sgx_tcrypto
SGX_PTHREAD_LIB_NAME := sgx_pthread
SGX_SSL_LIB_NAME := sgx_tsgxssl
SGX_SSL_LIB_PATH := $(SGX_SSL)/lib64

ENCLAVE_C_SOURCES := $(wildcard enclave/*.c)
ENCLAVE_INCLUDE_PATHS := -Iinclude -Ienclave -I$(SGX_SDK)/include -I$(SGX_SDK)/include/tlibc -I$(SGX_SSL)/include

ENCLAVE_CFLAGS := $(ENCLAVE_INCLUDE_PATHS) -nostdinc -fvisibility=hidden -fpie -ffunction-sections -fdata-sections $(MITIGATION_CFLAGS)
CC_BELOW_4_9 := $(shell expr "`$(CC) -dumpversion`" \< "4.9")
ifeq ($(CC_BELOW_4_9), 1)
	ENCLAVE_CFLAGS += -fstack-protector
else
	ENCLAVE_CFLAGS += -fstack-protector-strong
endif

# Enable the security flags
ENCLAVE_SECURITY_LINK_FLAGS := -Wl,-z,relro,-z,now,-z,noexecstack

# To generate a proper enclave, it is recommended to follow below guideline to link the trusted libraries:
#    1. Link sgx_trts with the `--whole-archive' and `--no-whole-archive' options,
#       so that the whole content of trts is included in the enclave.
#    2. For other libraries, you just need to pull the required symbols.
#       Use `--start-group' and `--end-group' to link these libraries.
# Do NOT move the libraries linked with `--start-group' and `--end-group' within `--whole-archive' and `--no-whole-archive' options.
# Otherwise, you may get some undesirable errors.
ENCLAVE_LINK_FLAGS := $(MITIGATION_LDFLAGS) $(ENCLAVE_SECURITY_LINK_FLAGS) \
    -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_TRUSTED_LIBRARY_PATH) -L$(SGX_SSL_LIB_PATH)\
	-Wl,--whole-archive -l$(TRTS_LIB_NAME) -Wl,--no-whole-archive \
	-Wl,--start-group -l$(SGX_SSL_LIB_NAME) -l$(SGX_SSL_LIB_NAME)_crypto \
	-lsgx_tstdc -lsgx_tcxx -l$(SGX_PTHREAD_LIB_NAME) \
	-l$(CRYPTO_LIB_NAME) -l$(SERVICE_LIB_NAME) -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0 -Wl,--gc-sections   \
	-Wl,--version-script=enclave/enclave.lds

ENCLAVE_C_OBJS := $(sort $(ENCLAVE_C_SOURCES:.c=.o))

ENCLAVE_NAME := enclave.so
SIGNED_ENCLAVE_NAME := enclave.signed.so
ENCLAVE_CONFIG_FILE := enclave/enclave.config.xml

ifeq ($(SGX_MODE), HW)
ifeq ($(SGX_DEBUG), 1)
	BUILD_MODE = HW_DEBUG
else ifeq ($(SGX_PRERELEASE), 1)
	BUILD_MODE = HW_PRERELEASE
else
	BUILD_MODE = HW_RELEASE
endif
else
ifeq ($(SGX_DEBUG), 1)
	BUILD_MODE = SIM_DEBUG
else ifeq ($(SGX_PRERELEASE), 1)
	BUILD_MODE = SIM_PRERELEASE
else
	BUILD_MODE = SIM_RELEASE
endif
endif


.PHONY: all target run
all: clean .config_$(BUILD_MODE)_$(SGX_ARCH)
	$(MAKE) target

ifeq ($(BUILD_MODE), HW_RELEASE)
target:  $(APP_NAME) $(ENCLAVE_NAME)
	@echo "The project has been built in release hardware mode."
	@echo "Please sign the $(ENCLAVE_NAME) first with your signing key before you run the $(APP_NAME) to launch and access the enclave."
	@echo "To sign the enclave use the command:"
	@echo "   $(SGX_ENCLAVE_SIGNER) sign -key <your key> -enclave $(ENCLAVE_NAME) -out <$(SIGNED_ENCLAVE_NAME)> -config $(ENCLAVE_CONFIG_FILE)"
	@echo "You can also sign the enclave using an external signing tool."
	@echo "To build the project in simulation mode set SGX_MODE=SIM. To build the project in prerelease mode set SGX_PRERELEASE=1 and SGX_MODE=HW."


else
target: $(APP_NAME) $(SIGNED_ENCLAVE_NAME)
ifeq ($(BUILD_MODE), HW_DEBUG)
	@echo "The project has been built in debug hardware mode."
else ifeq ($(BUILD_MODE), SIM_DEBUG)
	@echo "The project has been built in debug simulation mode."
else ifeq ($(BUILD_MODE), HW_PRERELEASE)
	@echo "The project has been built in pre-release hardware mode."
else ifeq ($(BUILD_MODE), SIM_PRERELEASE)
	@echo "The project has been built in pre-release simulation mode."
else
	@echo "The project has been built in release simulation mode."
endif

endif

run: all
ifneq ($(BUILD_MODE), HW_RELEASE)
	$(CURDIR)/$(APP_NAME)
	@echo "RUN  =>  $(APP_NAME) [$(SGX_MODE)|$(SGX_ARCH), OK]"
endif

.config_$(BUILD_MODE)_$(SGX_ARCH):
	rm -f .config_* $(APP_NAME) $(ENCLAVE_NAME) $(SIGNED_ENCLAVE_NAME) $(APP_C_OBJS) app/enclave_u.* $(ENCLAVE_C_OBJS) enclave/enclave_t.*
	touch .config_$(BUILD_MODE)_$(SGX_ARCH)

######## App Objects ########

app/enclave_u.h: $(SGX_EDGER8R) enclave/enclave.edl
	cd app && $(SGX_EDGER8R) --untrusted ../enclave/enclave.edl --search-path ../enclave --search-path $(SGX_SDK)/include

app/enclave_u.c: app/enclave_u.h

app/%.o: app/%.c app/enclave_u.h
	$(CC) $(SGX_COMMON_CFLAGS) $(APP_CFLAGS) -c $< -o $@

$(APP_NAME): app/enclave_u.o $(APP_C_OBJS)
	$(CC) $^ -o $@ $(APP_LINK_FLAGS)

######## Enclave Objects ########

enclave/enclave_t.h: $(SGX_EDGER8R) enclave/enclave.edl
	cd enclave && $(SGX_EDGER8R) --trusted ../enclave/enclave.edl --search-path ../enclave --search-path $(SGX_SDK)/include

enclave/enclave_t.c: enclave/enclave_t.h

enclave/%.o: enclave/%.c enclave/enclave_t.h
	$(CC) $(SGX_COMMON_CFLAGS) $(ENCLAVE_CFLAGS) -c $< -o $@

.PHONY: mkfs
mkfs: mkfs/mkfs.c
	$(CC) -Iinclude -Ienclave -Wall mkfs/mkfs.c -lcrypto -g -o mkfs/mkfs
	./mkfs/mkfs

$(ENCLAVE_NAME): enclave/enclave_t.o $(ENCLAVE_C_OBJS)
	$(CC) -Iinclude -Ienclave -Wall mkfs/mkfs.c -lcrypto -g -o mkfs/mkfs
	./mkfs/mkfs
	$(CC) $^ -o $@ $(ENCLAVE_LINK_FLAGS)

$(SIGNED_ENCLAVE_NAME): $(ENCLAVE_NAME)
	$(SGX_ENCLAVE_SIGNER) sign -key enclave/enclave_private.pem -enclave $(ENCLAVE_NAME) -out $@ -config $(ENCLAVE_CONFIG_FILE)

.PHONY: clean

clean:
	rm -f .config_* $(APP_NAME) $(ENCLAVE_NAME) $(SIGNED_ENCLAVE_NAME) $(APP_C_OBJS) app/enclave_u.* $(ENCLAVE_C_OBJS) enclave/enclave_t.*
