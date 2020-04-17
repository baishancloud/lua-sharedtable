# $^ prerequisite
# $< first prerequisite
# $@ target

# usage:
#     make ** build=release|test debug=0|1 debug_unittest=0|1 mem=small|normal
#
st_version_major = 0
st_version_minor = 1
st_version_release = 0
st_version_soname = $(st_version_major)
st_version_full = $(st_version_major).$(st_version_minor).$(st_version_release)

DEBUG_FLAGS_1 = -fprofile-arcs -ftest-coverage -g -D ST_DEBUG -O2 -D DNDEBUG
UNITTEST_FLAGS_1 = -D ST_DEBUG_UNITTEST
MEM_FLAGS_normal = -D MEM_NORMAL
MEM_FLAGS_small = -D MEM_SMALL

ifndef build
	build = release
endif

BUILD_TYPES = release test
ifeq ($(findstring $(build), $(BUILD_TYPES)),)
$(error unknown build type, valid value: $(BUILD_TYPES))
endif

ifndef debug
	ifeq ($(build), release)
		debug = 0
	else
		debug = 1
	endif
endif

ifndef debug_unittest
	ifeq ($(build), release)
		debug_unittest = 0
	else
		debug_unittest = 1
	endif
endif

ifndef mem
	ifeq ($(build), release)
		mem = normal
	else
		mem = small
	endif
endif

ifeq ($(findstring $(debug), 0 1),)
$(error invalid debug value $(debug), on: 1, off: 0)
endif

ifeq ($(findstring $(debug_unittest), 0 1),)
$(error invalid debug value $(debug_unittest), on: 1, off: 0)
endif

ifeq ($(findstring $(mem), small normal),)
$(error invalid debug value $(mem), valid value: small normal)
endif


BUILD_EXTRA_CFLAGS += $(DEBUG_FLAGS_$(debug))
BUILD_EXTRA_CFLAGS += $(UNITTEST_FLAGS_$(debug_unittest))
BUILD_EXTRA_CFLAGS += $(MEM_FLAGS_$(mem))


CC        = gcc
BASE_DIR ?= ..
CFLAGS   ?= -g -O2 -fPIC -Wall -Werror -std=c11 -D _GNU_SOURCE $(BUILD_EXTRA_CFLAGS)

C_INCLUDE_PATH = $(BASE_DIR):$(BASE_DIR)/inc
export C_INCLUDE_PATH

srcs = $(src)
objs = $(srcs:.c=.o)
debug_objs = $(srcs:.c=.gcda) $(srcs:.c=.gcno)

# Translate mods to absolute path;
# Add ".a" trailer;
deps_a = $(join $(deps:%=$(BASE_DIR)/%/),$(deps:=.a))

test_src = $(srcs) $(test_exec).c
test_objs = $(test_src:.c=.o)
test_debug_objs = $(test_src:.c=.gcda) $(test_src:.c=.gcno)

# $(1): a list of absolute path names of dependent static lib files, like xxx.a
# $(2): execute command when file exists or not, 1: true, 0: false
# $(3): make action like `clean`, ``, `test`. `` is for basic make
define make_each_dep
	@for dep in $(1); do              \
		[ -f $${dep} ];               \
		if [ $${?} -ne $(2) ]; then   \
			cd $$(dirname $${dep}) && \
				$(MAKE) $(3) &&       \
				cd -;                 \
		fi;                           \
	done;
endef

# t: clean test valgrind
t: test

b: clean bench

dylib: all
ifneq ($(target_dylib),)
	$(CC) -fPIC -shared -o $(target_dylib).$(st_version_full)          \
		-Wl,-soname,$(target_dylib).$(st_version_soname)               \
		-Wl,--whole-archive $(target) $(deps_a) -Wl,--no-whole-archive $(libs:%=-l% )
	@ln -sf $(target_dylib).$(st_version_full) $(target_dylib)
else
	@echo "no dylib target specified"
endif

valgrind: $(target) $(test_exec)
	# valgrind --leak-check=full --dsymutil=yes $(test_exec) 2>&1 | grep -C10 --color xpj
	valgrind --leak-check=full --dsymutil=yes $(test_exec) 2>&1

all: $(target)
	# 3rd arg is empty to trigger simple `make` command
	$(call make_each_dep, $(deps_a), 0, all)

clean:
	-@rm $(objs) $(debug_objs) 2>/dev/null
	-@rm $(target) 2>/dev/null
	-@rm $(test_objs) $(test_debug_objs) 2>/dev/null
	-@rm $(test_exec) 2>/dev/null
	-@rm $(target_dylib) 2>/dev/null
	-@rm $(target_dylib).$(st_version_full) 2>/dev/null
	$(call make_each_dep, $(deps_a), 1, clean)
	-@echo "clean done ----------------"

# bench: $(target) $(test_exec)
bench: $(test_exec)
	./$(test_exec) bench

# test: $(target) $(test_exec)
test: $(target) $(test_exec) dylib
	./$(test_exec) test

$(test_exec): $(test_objs)
	$(call make_each_dep, $(deps_a), 0, test)
	$(CC) $(CFLAGS) $(cflags) $(libs:%=-l% ) -o $@ $^ $(deps_a)

$(objs): $(BASE_DIR)/inc/*.h

$(test_objs): $(BASE_DIR)/inc/*.h $(BASE_DIR)/unittest/*.h

# General rules
%.a: $(objs)
	ar -rcs $@ $(objs)

.c.o:
	$(CC) $(CFLAGS) $(cflags) -o $@ -c $<

.h.c:
