# $^ prerequisite
# $< first prerequisite
# $@ target

BASE_DIR := $(CURDIR)
export BASE_DIR

submod = binary     \
		 btree      \
		 robustlock \
		 list       \
		 rbtree     \
		 array      \
		 bitmap     \
		 atomic     \
		 region     \
		 pagepool   \
		 slab       \
		 table      \
		 gc         \
		 version    \
		 capi       \
		 luaapi

test:
	for mod in $(submod); do ( cd $$mod && $(MAKE) test; ) done

dylib:
	cd sharetable && make dylib && cd -

clean:
	for mod in $(submod); do ( cd $$mod && $(MAKE) clean; ) done
