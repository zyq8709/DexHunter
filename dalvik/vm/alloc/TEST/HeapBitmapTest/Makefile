.PHONY: all
all: runtest

$(shell mkdir -p out)

CC := gcc
CFLAGS := -g -Wall -Werror
#CFLAGS += -O2

out/main.o: main.c ../../HeapBitmap.h
	$(CC) $(CFLAGS) -c $< -o $@ -I ../..

out/HeapBitmap.o: ../../HeapBitmap.c ../../HeapBitmap.h include/cutils/ashmem.h include/Dalvik.h
	$(CC) $(CFLAGS) -c $< -o $@ -I ../.. -I include

out/hbtest: out/main.o out/HeapBitmap.o out/clz.o
	$(CC) $^ -o $@

.PHONY: runtest
runtest: out/hbtest
	out/hbtest

.PHONY: clean
clean:
	rm -rf out
