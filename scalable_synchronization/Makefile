default: all

CC = gcc
CFLAGS = -std=c11 -O2 -D_GNU_SOURCE -mavx2
CLIBS = -lpthread

O = build

$(O):
	mkdir $(O)

$(O)/mutex.o:$(O) mutex.c synchronize.h
	$(CC) $(CFLAGS) -c mutex.c -o $(O)/mutex.o

$(O)/barrier.o:$(O) barrier.c synchronize.h
	$(CC) $(CFLAGS) -c barrier.c -o $(O)/barrier.o

$(O)/thread_utils.o:$(O) thread_utils.c synchronize.h
	$(CC) $(CFLAGS) -c thread_utils.c -o $(O)/thread_utils.o

$(O)/test_empty_section.o:$(O) test.c synchronize.h
	$(CC) $(CFLAGS) -DEMPTY_SECTION -c test.c -o $(O)/test_empty_section.o

$(O)/test_empty_section:$(O)/thread_utils.o $(O)/mutex.o $(O)/barrier.o $(O)/test_empty_section.o
	$(CC) $(O)/test_empty_section.o $(O)/thread_utils.o $(O)/mutex.o $(O)/barrier.o -o $(O)/test_empty_section $(CLIBS)

$(O)/test_small_section.o:$(O) test.c synchronize.h
	$(CC) $(CFLAGS) -c test.c -o $(O)/test_small_section.o

$(O)/test_small_section:$(O)/thread_utils.o $(O)/mutex.o $(O)/barrier.o $(O)/test_small_section.o
	$(CC) $(O)/test_small_section.o $(O)/thread_utils.o $(O)/mutex.o $(O)/barrier.o -o $(O)/test_small_section $(CLIBS)

all:$(O)/test_small_section $(O)/test_empty_section

run:$(O)/test_small_section
	./$(O)/test_small_section 4

clean:
	rm -rf $(O)
