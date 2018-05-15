src          = capi.c
target       = capi.a
target_dylib = libst_capi.so
libs         = pthread rt
deps         = pagepool   \
			   rbtree     \
			   array      \
			   region     \
			   slab       \
			   bitmap     \
			   robustlock \
			   gc         \
			   table      \
			   str        \
			   util		  \
			   version

test_exec    = test_capi

BASE_DIR ?= $(CURDIR)/..
include $(BASE_DIR)/def.mk
