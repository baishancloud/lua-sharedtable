<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
#   Table of Content

- [Name](#name)
- [Status](#status)
- [Description](#description)
  - [Module List](#module-list)
- [Install](#install)
- [Usage](#usage)
- [Configuration](#configuration)
- [Test](#test)
- [Author](#author)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

#   Name

lua-sharedtable:

lua-sharetable is a key value store table and can shared between processes.

Features:

- Table is associative array, that can be indexed not only with number but also with string.

- Table value can not only be integer, string, float, but also table reference.

- No fixed size, you can add as many elements as you want to a table dynamically.
    memory space is dynamic extended.

- Low memory fragment, in bottom layer, memory space split into region, pages, slab chunk.
    it can choose the best fit size to allocate.

- High memory allocation performance

    - memory space is occupied in init phase, but not bind physical page, so no memory use.
        when table use space, it just trigger page fault
        to bind virtual memory with physical page, so no system call to delay.

    - there are some kind of chunks in slab pool, and some kind of pages in page pool,
        so alloc memory to user, just to search in array or tree data structure.

- No redundant memory occupied, when free memory reach threshold,
    the memory will release to system through unbind virtual memory from physical page.

- No afraid process died, if process died, it's share memory will be release.
    even the died process hold the lock, the lock will be recovered.


#   Status

This library is in beta phase.

It has been used in our object storage service.

#   Description

##  Module List

There is a `README.md` for each module.

| name                           | description                                 |
| :--                            | :--                                         |
| [array](array)                 | dynamic and static array                    |
| [atomic](atomic)               | atomic operation                            |
| [btree](btree)                 | btree operation                             |
| [binary](binary)               | binary operation                            |
| [bitmap](bitmap)               | bitmap operation                            |
| [list](list)                   | double linked list operation                |
| [rbtree](rbtree)               | red–black tree operation                    |
| [robustlock](robustlock)       | robustlock can avoid dead lock              |
| [str](str)                     | string utility                              |
| [region](region)               | manage system alloced big memory            |
| [pagepool](pagepool)           | manage pages which split from region        |


# Install

It requires gcc 4.9+ and c11 standard.

For now the latest centos still has no gcc-4.9 included.
We need to install gcc-4.9 with devtoolset-3.

To install gcc 4.9 on centos:

```
yum install centos-release-scl-rh -y
yum install devtoolset-3-gcc -y

```

To use gcc-4.9 in devtoolset-3, start a new bash with devtoolset-3 enabled:

```
scl enable devtoolset-3 bash
```

To enable devtoolset-3 for ever, add the following line into `.bashrc`:

```
source /opt/rh/devtoolset-3/enable
```

#   Usage

#   Configuration

#   Test

#   Author

- Zhang Yanpo (张炎泼) <xp@baishancloud.com>
- Chen Chuang (陈闯) <chuang.chen@baishancloud.com>
- Li Shulong (李树龙) <shulong.li@baishancloud.com>
