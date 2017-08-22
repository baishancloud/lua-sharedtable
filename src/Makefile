# $^ prerequisite
# $< first prerequisite
# $@ target

BASE_DIR := $(CURDIR)
export BASE_DIR

submod = binary btree lock

test:
	for mod in $(submod); do ( cd $$mod && $(MAKE) test; ) done

clean:
	for mod in $(submod); do ( cd $$mod && $(MAKE) clean; ) done
