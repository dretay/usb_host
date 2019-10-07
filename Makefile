ifeq ($(OS),Windows_NT)
  ifeq ($(shell uname -s),) # not in a bash-like shell
	CLEANUP = del /F /Q
	MKDIR = mkdir
  else # in a bash-like shell, like msys
	CLEANUP = rm -f
	MKDIR = mkdir -p
  endif
	TARGET_EXTENSION=.exe
else
	CLEANUP = rm -f
	MKDIR = mkdir -p
	TARGET_EXTENSION=out
endif

#explicit directories
BUILD_DIR = build/
RELEASE_DIR = release/
CONFIG_DIR = conf/
SRC_DIR = src/
LIB_DIR = lib/
TEST_DIR = test/

#derived directories
PROFILING_RESULTS_DIR = $(BUILD_DIR)profiling_results/
CPPCHECK_RESULTS_DIR = $(BUILD_DIR)cppcheck_results/
TEST_RESULTS_DIR = $(BUILD_DIR)test_results/
TEST_OBJ_DIR = $(BUILD_DIR)test/
TEST_RUNNER_DIR = $(TEST_DIR)test_runners/
INCLUDE_RELEASE_DIR = $(RELEASE_DIR)include/
SRC_RELEASE_DIR = $(RELEASE_DIR)src/

#project source files
SRCS := $(shell find $(LIB_DIR) $(SRC_DIR) -maxdepth 2 \( -iname "*.c" \))
HEADERS = $(shell find $(LIB_DIR) $(SRC_DIR) -maxdepth 2 \( -iname "*.h" \))
OBJS = $(SRCS:%=$(BUILD_DIR)%.o)
INC_DIRS := $(shell find $(LIB_DIR) -maxdepth 1 -type d)

#unity testing files
TESTS = $(patsubst $(SRC_DIR)%.c,$(TEST_DIR)Test%.c,$(SRCS) )
RUNNERS = $(patsubst $(TEST_DIR)%.c,$(TEST_RUNNER_DIR)%.c,$(TESTS) )
TEST_RESULTS = $(patsubst $(TEST_DIR)Test%.c,$(TEST_RESULTS_DIR)Test%.txt,$(TESTS) )
TEST_OBJS = $(TESTS:%=$(BUILD_DIR)%.o)
UNITY_ROOT=/home/drew/src/Unity

#valgrind stuff
VALGRIND = /usr/bin/valgrind
VALGRIND_SUPPS = $(CONFIG_DIR)valgrind.memcheck.supp
PROFILING_RESULTS = $(patsubst $(TEST_DIR)Test%.c,$(PROFILING_RESULTS_DIR)Test%.out,$(TESTS) )

#cppcheck
CPPCHECK = cppcheck
CPPCHECK_FILES := $(shell find $(SRC_DIR) -maxdepth 2 \( -iname "*.c" \))
CPPCHECK_FLAGS = -q --enable=all --inconclusive --suppress=missingIncludeSystem
CPPCHECK_RESULTS = $(CPPCHECK_FILES:%=$(CPPCHECK_RESULTS_DIR)%.txt)

#misc variables
DIRECTIVES = -DLOG_USE_COLOR -DUNITY_OUTPUT_COLOR
FLAGS = -fPIC
INC_FLAGS := $(addprefix -I,$(INC_DIR)) -I$(UNITY_ROOT)/src -I./src
CURRENT_DIR = $(notdir $(shell pwd))
CP = cp
CFLAGS = $(INC_FLAGS) $(FLAGS) $(DIRECTIVES) --std=gnu99

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	CFLAGS +=   -O0 -g3
	LDFLAGS = -shared
else
	CFLAGS +=  -O3
	LDFLAGS = -shared
# 	ARFLAGS = rcs
endif

PLATFORM ?= LINUX
ifeq ($(PLATFORM),$(filter $(PLATFORM),NRF51 STM32F103))
	#set instruction set to thumb... debatably arm might be valid too i guess but I'm too lazy and this code
	#doesn't really have zero wait to worry about...
	CFLAGS += -mthumb
	# set the ABI to aapcs (current standard, needs to be set consistently or linker will explode?)
	CFLAGS += -mabi=aapcs
	#softcore floating point
	CFLAGS += -mfloat-abi=soft
	# keep every function in separate section. This will allow linker to dump unused functions
  	CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing -fstack-usage
	# don't use functions built-into gcc
	#use embedded libc
	CFLAGS += --specs=nano.specs

	ARFLAGS += --target elf32-littlearm
	CC = arm-none-eabi-gcc
	LD = arm-none-eabi-ld
	AR = arm-none-eabi-ar
else
	DIRECTIVES += -DLINUX
	CC = gcc
	LD = ld
	AR = ar
endif

ifeq ($(PLATFORM), NRF51)
	#setting this wrong (like m4) means weird stuff like hard faults when an int goes negative...
	CPU	= cortex-m0
	CFLAGS += -mcpu=$(CPU)
	#software floating point
	CFLAGS +=-mfloat-abi=soft
endif

.PHONY: all
.PHONY: release
.PHONY: test
.PHONY: profile
.PHONY: clean
.PHONY: cppcheck
.PHONY: includes
.PHONY: copy


all: $(RUNNERS) $(OBJS) cppcheck

includes:
	$(MKDIR) $(INCLUDE_RELEASE_DIR)
	cp $(HEADERS) $(INCLUDE_RELEASE_DIR)

copy: #release
	$(CLEANUP)r /mnt/windows/include/$(CURRENT_DIR)
	$(MKDIR) /mnt/windows/include/$(CURRENT_DIR)
	cp $(HEADERS) /mnt/windows/include/$(CURRENT_DIR)
	cp $(SRCS) /mnt/windows/include/$(CURRENT_DIR)
	#cp $(RELEASE_DIR)lib$(CURRENT_DIR).a /mnt/windows/include


release: all includes $(RELEASE_DIR)lib$(CURRENT_DIR).a

test: all $(TEST_OBJS) $(TEST_RESULTS) cppcheck
	@echo ""
	@echo "-----------------------ANALYSIS AND TESTING SUMMARY-----------------------"
	@echo `find $(TEST_RESULTS_DIR) -type f -exec grep IGNORE {} \;|wc -l` "tests ignored"
	@echo "`find $(TEST_RESULTS_DIR) -type f -exec grep IGNORE {} \;`"
	@echo `find $(TEST_RESULTS_DIR) -type f -exec grep FAIL {} \;|wc -l` "tests failed"
	@echo "`find $(TEST_RESULTS_DIR) -type f -exec grep FAIL {} \;`"
	@echo `find $(TEST_RESULTS_DIR) -type f -exec grep PASS {} \;|wc -l` "tests passed"
	@echo ""
	@echo "`grep -Poh 'ERROR SUMMARY:\K ([0-9]+)' $(TEST_RESULTS_DIR)*| awk '{ SUM += $$1} END { print SUM }'` memory leak(s) detected"
	@echo ""
	@echo `find $(CPPCHECK_RESULTS_DIR) -type f -exec grep warning {} \;|wc -l` "code warnings"
	@echo `find $(CPPCHECK_RESULTS_DIR) -type f -exec grep warning {} \;`
	@echo `find $(CPPCHECK_RESULTS_DIR) -type f -exec grep error {} \;|wc -l` "code errors"
	@echo "`find $(CPPCHECK_RESULTS_DIR) -type f -exec grep error {} \;`"

profile: all $(PROFILING_RESULTS)

$(RELEASE_DIR)lib$(CURRENT_DIR).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

#generate profiling data
$(PROFILING_RESULTS_DIR)%.out: $(BUILD_DIR)%.c.o.$(TARGET_EXTENSION)
	$(MKDIR) $(dir $@)
	-$(VALGRIND) --tool=callgrind --callgrind-out-file=$@  $< > /dev/null 2>&1

# assembly
$(BUILD_DIR)%.s.o: %.s
	$(MKDIR) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

#execute cppcheck
cppcheck: $(CPPCHECK_RESULTS)
$(CPPCHECK_RESULTS_DIR)%.c.txt: %.c
	$(MKDIR) $(dir $@)
	$(CPPCHECK) $(INC_FLAGS) $(DIRECTIVES) $(CPPCHECK_FLAGS) $< > $@ 2>&1

# c source
$(BUILD_DIR)%.c.o: %.c
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

#execute tests
$(TEST_RESULTS_DIR)%.txt: $(BUILD_DIR)%.c.o.$(TARGET_EXTENSION)
	$(MKDIR) $(dir $@)
	-$(VALGRIND) --suppressions=$(VALGRIND_SUPPS) --gen-suppressions=all --tool=memcheck --leak-check=full $< > $@ 2>&1

#build the test runners
$(BUILD_DIR)%.c.o.$(TARGET_EXTENSION): $(TEST_OBJ_DIR)%.c.o
	$(CC) -g -o $@ $^ $(CFLAGS) $(OBJS) $(UNITY_ROOT)/src/unity.c $(TEST_RUNNER_DIR)$(basename $(notdir $<))

#unity test runners
$(TEST_RUNNER_DIR)%.c:: $(TEST_DIRS)%.c
	$(MKDIR) $(dir $@)
	ruby $(UNITY_ROOT)/auto/generate_test_runner.rb $< $@

clean:
	$(CLEANUP)r $(BUILD_DIR)
	$(CLEANUP)r $(RELEASE_DIR)

.PRECIOUS: $(TEST_RESULTS_DIR)%.txt
.PRECIOUS: $(PROFILING_RESULTS_DIR)%.txt
.PRECIOUS: $(BUILD_DIR)%.c.o.out
