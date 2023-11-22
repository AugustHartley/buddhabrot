CC=gcc
PKGCONF=pkgconf
PKGS=libpng
CFLAGS=-pthread -O2 -ffast-math -g $(shell $(PKGCONF) --cflags $(PKGS))
LDFLAGS=-lm -lmvec $(shell $(PKGCONF) --libs $(PKGS))

OBJ=bin/main.o bin/buddhabrot.o bin/buddhabrot.avx2.o
DEP=$(patsubst bin/%.o,bin/%.d,$(OBJ))

buddhabrot: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

bin:
	mkdir bin

bin/%.o: src/%.c | bin
	$(CC) $(CFLAGS) -MP -MMD -c $< -o $@

-include $(DEP)

.PHONY: run clean profile

profile: clean
	$(MAKE) buddhabrot CFLAGS="$(CFLAGS) -fprofile-generate"
	time ./buddhabrot
	rm -rf buddhabrot bin/*.o
	$(MAKE) buddhabrot CFLAGS="$(CFLAGS) -fprofile-use"

clean:
	rm -rf buddhabrot bin
