src          = luast.c
target       = luast.a
target_dylib = libluast.so
libs         = pthread rt lua
deps         = capi pagepool rbtree array region slab bitmap robustlock gc table str

test_exec    = test_luast

BASE_DIR ?= $(CURDIR)/..
include $(BASE_DIR)/def.mk
