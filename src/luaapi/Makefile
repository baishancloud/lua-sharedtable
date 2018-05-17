src          = luaapi.c
target       = luaapi.a
target_dylib = libluast.so
libs         = pthread rt lua
deps         = capi       \
			   pagepool   \
			   rbtree     \
			   array      \
			   region     \
			   slab       \
			   bitmap     \
			   robustlock \
			   gc         \
			   table      \
			   str        \
			   version	  \
			   util

test_exec    = test_luaapi

BASE_DIR ?= $(CURDIR)/..
include $(BASE_DIR)/def.mk
