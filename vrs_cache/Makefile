#sudo apt-get install libomp-dev
#sudo apt-get install libcrypto++-dev
#sudo apt-get install libboost-all-dev
#sudo apt-get install libiomp5 (if clang in linux, use -lomp replace -lgomp)

ifeq ($(OS),Windows_NT)
	OS_TYPE := Windows
else
	UNAME := $(shell uname -s)
	ifeq ($(UNAME),Linux)
		OS_TYPE := Linux
	else ifeq ($(UNAME),Darwin)
		OS_TYPE := Darwin
	else
		OS_TYPE := Unknown
	endif
endif

CXX ?= $(shell which g++)
ifdef CXX11_HOME
  CXX := LD_LIBRARY_PATH=${CXX11_HOME}/lib $(CXX11_HOME)/bin/g++
endif

HOME = ../linux
THIRD_PARTY_HOME = ../thirdparty
YAS_HOME = ../thirdparty/yas/include
PUB_HOME = ../public
SNARK_HOME = ../depends/libsnark
FF_HOME = $(SNARK_HOME)/depends/libff
MCL_HOME = $(SNARK_HOME)/depends/mcl/include

INC_FLAGS :=  \
 -I /usr/local/include \
 -I /usr/include \
 -isystem $(SNARK_HOME) \
 -isystem $(MCL_HOME) \
 -isystem $(FF_HOME) \
 -isystem $(YAS_HOME) \
 -isystem $(THIRD_PARTY_HOME) \
 -I $(PUB_HOME) \
 -I. \
 -I.. \

ifdef CXX11_HOME
  INC_FLAGS += -I $(CXX11_HOME)/include
endif

CXXFLAGS := \
 -g3 \
 -fPIC \
 -std=c++17 \
 -Wall \
 -Wextra \
 -gdwarf-2 \
 -gstrict-dwarf \
 -Wno-parentheses \
 -Wdeprecated-declarations \
 -fmerge-all-constants  \
 -mtune=native \
 -march=native \
 $(INC_FLAGS) \

SNARK_FLAGS = \
 -DBN_SUPPORT_SNARK=1 \
 -DNO_PT_COMPRESSION=1 \
 -DCURVE_MCL_BN128 \
 -DMULTICORE=1 \
 -DUSE_ASM \
 -DNO_PROCPS \

MCL_FLAGS := \
 -DMCL_DONT_USE_OPENSSL

RELEASE_FLAGS := \
 -O3 \
 -DNDEBUG \
 -DLINUX \

DEBUG_FLAGS := \
 -O0 \
 -D_DEBUG \
 -DLINUX \
 
LDFLAGS += \
 -fmerge-all-constants

LIBS := \
 -lpthread \
 -lrt \
 -lgmpxx \
 -lgmp \
 -lcryptopp \
 -lboost_filesystem \
 -lboost_system \
 -lboost_iostreams \
 -lboost_program_options \

ifdef USE_TBB
LIBS += -ltbb
CXXFLAGS += -DUSE_TBB
endif

ifeq ($(OS_TYPE),Darwin)
  TMPVAR := $(CXXFLAGS)
  CXXFLAGS := $(filter-out -fopenmp, $(TMPVAR))

  TMPVAR := $(SNARK_FLAGS)
  SNARK_FLAGS := $(filter-out -DMULTICORE=1, $(TMPVAR))

  TMPVAR := $(SNARK_FLAGS)
  SNARK_FLAGS := $(filter-out -DUSE_ASM, $(TMPVAR))

  TMPVAR := $(LIBS)
  TMPVAR := $(filter-out -lgomp, $(TMPVAR))
  LIBS := $(filter-out -lrt, $(TMPVAR))
endif

BUILD := ./build/self
BUILDPUB := ./build/public

$(shell mkdir -p $(HOME)/bin)
$(shell mkdir -p $(BUILD))
$(shell mkdir -p $(BUILDPUB))

DIR := . 

SRC := $(foreach d, $(DIR), $(wildcard $(d)/*.cc))

SRC += ../public/ecc.cc

RELEASE_OBJ := $(patsubst %.cc, %.o, $(SRC))

DEBUG_OBJ := $(patsubst %.cc, %.d.o, $(SRC))

RELEASE_BIN := vrs_cache_tool
DEBUG_BIN := vrs_cache_toold

all: $(RELEASE_BIN) $(DEBUG_BIN)

%.o : %.cc
	$(CXX) -c $^ $(CXXFLAGS) $(RELEASE_FLAGS) $(MCL_FLAGS) $(SNARK_FLAGS) -o $(BUILD)/$@


%.d.o : %.cc
	$(CXX) -c $^ $(CXXFLAGS) $(DEBUG_FLAGS) $(MCL_FLAGS) $(SNARK_FLAGS) -o $(BUILD)/$@


$(DEBUG_BIN) : $(DEBUG_OBJ)
	$(CXX) $(addprefix $(BUILD)/, $^) -o $(HOME)/bin/$@ $(LDFLAGS) $(LIBS)

$(RELEASE_BIN) : $(RELEASE_OBJ)
	$(CXX) $(addprefix $(BUILD)/, $^) -o $(HOME)/bin/$@ $(LDFLAGS) $(LIBS)

clean:
	find . -name "*.o" -delete
	rm -rf $(HOME)/bin/$(RELEASE_BIN) $(HOME)/bin/$(DEBUG_BIN)
	rm -rf build

.PHONY :
