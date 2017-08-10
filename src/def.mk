# $^ prerequisite
# $< first prerequisite
# $@ target

# usage:
#     make ** DEBUG=1 DEBUG_UNITTEST=1

DEBUG_FLAGS    = $(if $(DEBUG), -fprofile-arcs -ftest-coverage -g -D ST_DEBUG, -O2 -D DNDEBUG)
# DEBUG_FLAGS    = $(if $(DEBUG), -fprofile-arcs -ftest-coverage -g , -O2 -D DNDEBUG)
UNITTEST_FLAGS = $(if $(DEBUG_UNITTEST), -D ST_DEBUG_UNITTEST)

CC        = gcc
BASE_DIR ?= ..
CFLAGS   ?= -g -Wall -Werror -std=c11 -pthread -D_GNU_SOURCE $(DEBUG_FLAGS)

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

# t: clean test valgrind
t: clean test

b: clean bench

valgrind: $(target) $(test_exec)
	# valgrind --leak-check=full --dsymutil=yes $(test_exec) 2>&1 | grep -C10 --color xpj
	valgrind --leak-check=full --dsymutil=yes $(test_exec) 2>&1

all: $(target)
	@for dep in $(deps); do ( cd $(BASE_DIR)/$$dep && $(MAKE); ) done

clean:
	@for dep in $(deps); do ( cd $(BASE_DIR)/$$dep && $(MAKE) clean; ) done
	-@rm $(objs) $(debug_objs) 2>/dev/null
	-@rm $(target) 2>/dev/null
	-@rm $(test_objs) $(test_debug_objs) 2>/dev/null
	-@rm $(test_exec) 2>/dev/null
	-@echo "clean done ----------------"

# bench: $(target) $(test_exec)
bench: $(test_exec)
	./$(test_exec) bench

# test: $(target) $(test_exec)
test: $(test_exec)
	./$(test_exec) test

$(test_exec): $(test_objs)
	@for dep in $(deps); do ( cd $(BASE_DIR)/$$dep && echo make $$dep && $(MAKE) test; ) done
	$(CC) $(CFLAGS) $(libs:%=-l% ) -o $@ $^ $(deps_a)

$(objs): $(BASE_DIR)/inc/*.h

$(test_objs): $(BASE_DIR)/inc/*.h $(BASE_DIR)/unittest/*.h

# General rules
%.a: $(objs)
	ar -rcs $@ $(objs)

.c.o:
	$(CC) $(CFLAGS) $(UNITTEST_FLAGS) -o $@ -c $<

.h.c:
