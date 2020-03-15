## -*- Makefile -*-

CCC = gcc
CXX = g++
LD = g++
CFLAGS = -g
CCFLAGS = $(CFLAGS)
CXXFLAGS = $(CFLAGS)
RM = rm -f
RMDIR = rm -f -r
MKDIR = mkdir -p

TARGETDIR=build/
TARGET=lib_test_runner

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))
remove_slash = $(patsubst %/,%,$1)
SOURCE_PATH = $(call remove_slash,$(abspath $(mkfile_dir)/../..))

BUILD_PATH=$(TARGETDIR)test/

# Recursive wildcard function
rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

# enumerates files in the filesystem and returns their path relative to the project root
# $1 the directory relative to the project root
# $2 the pattern to match, e.g. *.cpp
target_files = $(patsubst $(SOURCE_PATH)/%,%,$(call rwildcard,$(SOURCE_PATH)/$1,$2))

# here_files is a non-recursive file search. target_files is recursive.
here_files = $(patsubst $(SOURCE_PATH)/%,%,$(wildcard $(SOURCE_PATH)/$1/$2))

# add all tests
CPPSRC += $(call here_files,lib/test,*.cpp)

# add all lib source files
INCLUDE_DIRS += $(SOURCE_PATH)/lib/inc
CPPSRC += $(call here_files,lib/src,*.cpp)

# set cnl as system includes to suppress warnings
CPPFLAGS += -isystem $(SOURCE_PATH)/lib/cnl/include

#  catch 2
INCLUDE_DIRS += $(SOURCE_PATH)/platform/spark/device-os/third_party/catch2/catch2/single_include/catch2

# include boost
ifeq ($(BOOST_ROOT),)
$(error BOOST_ROOT not set. Download boost and add BOOST_ROOT to your environment variables.)
endif
CPPFLAGS += -isystem $(BOOST_ROOT)

CFLAGS += $(patsubst %,-I%,$(INCLUDE_DIRS)) -I.
CFLAGS += -ffunction-sections -Wall
CFLAGS += -ffunction-sections -Wall
CFLAGS += -Wno-deprecated
CFLAGS += -Wnull-dereference 
CPPFLAGS += -fdiagnostics-show-template-tree -fno-elide-type # GCC8 required
CFLAGS += -Wextra
CPPFLAGS += -Wextra-semi
CFLAGS += -Wpedantic -Wvla -Wswitch-enum -fvar-tracking-assignments 
CFLAGS += -Wduplicated-cond -Wduplicated-branches -rdynamic

# Flag compiler error for [-Wdeprecated-declarations]
CFLAGS += -Werror=deprecated-declarations

# the following warnings can help find opportunities for impromevent in virtual functions
# Warn when virtual functions are overriden without override/override final specifier (requires gcc 5.1)
CPPFLAGS += -Wsuggest-override
# Warn when functions and classes can be marked final
CPPFLAGS += -Wsuggest-final-types
CPPFLAGS += -Wsuggest-final-methods

# Generate dependency files automatically.
CFLAGS += -MD -MP -MF $@.d
CFLAGS += -DDEBUG_BUILD
# OSX includes sys/wait.h which defines "wait"
CFLAGS += -D_SYS_WAIT_H_ -D_SYS_WAIT_H

CPPFLAGS += -std=gnu++14
CFLAGS += -pthread

# compile with coverage
CFLAGS += -g -fprofile-arcs -ftest-coverage
LDFLAGS += -lgcov

# don't generate warnings for system headers
CFLAGS += -Wno-system-headers

# set platform flag
CFLAGS += -DPLATFORM_ID=3

CFLAGS += $(patsubst %,-I%,$(INCLUDE_DIRS)) -I.

# Collect all object and dep files
ALLOBJ += $(addprefix $(BUILD_PATH), $(CSRC:.c=.o))
ALLOBJ += $(addprefix $(BUILD_PATH), $(CPPSRC:.cpp=.o))

ALLDEPS += $(addprefix $(BUILD_PATH), $(CSRC:.c=.o.d))
ALLDEPS += $(addprefix $(BUILD_PATH), $(CPPSRC:.cpp=.o.d))

all: runner

runner: $(TARGETDIR)$(TARGET)

$(TARGETDIR)$(TARGET) : $(BUILD_PATH) $(ALLOBJ)
	@echo Building target: $@
	@$(MKDIR) $(dir $@)
	@$(LD) $(CFLAGS) $(ALLOBJ) --output $@ $(LDFLAGS)
	@echo

$(BUILD_PATH): 
	$(MKDIR) $(BUILD_PATH)

# Tool invocations

# C compiler to build .o from .c in $(BUILD_DIR)
$(BUILD_PATH)%.o : $(SOURCE_PATH)/%.c
	@echo Building file: $<
	@$(MKDIR) $(dir $@)
	@$(CCC) $(CCFLAGS) -c -o $@ $<

# CPP compiler to build .o from .cpp in $(BUILD_DIR)
# Note: Calls standard $(CC) - gcc will invoke g++ as appropriate
$(BUILD_PATH)%.o : $(SOURCE_PATH)/%.cpp
	@echo Building file: $<
	@$(MKDIR) $(dir $@)
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

# Other Targets
clean:	
	$(RM) $(ALLOBJ) $(ALLDEPS) $(TARGETDIR)$(TARGET)
	$(RMDIR) $(TARGETDIR)	
	@echo

# print variable by invoking make print-VARIABLE as VARIABLE = the_value_of_the_variable
print-%  : ; @echo $* = $($*)

.PHONY: all clean runner
.SECONDARY:

# Include auto generated dependency files
-include $(ALLDEPS)



