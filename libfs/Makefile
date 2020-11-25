# Target library
lib := libfs.a
# Object files
objs := fs.o disk.o

# Define compilation toolchain
CC := gcc

# General gcc options
#CFLAGS	:= -Wall -Werror -Wextra
CFLAGS	:= -Wall -Werror
CFLAGS	+= -g

## Debug flag
ifneq ($(D),1)
CFLAGS	+= -O2
else
CFLAGS	+= -g
endif

## Dependency generation
CFLAGS += -MMD

ARCHIVE := ar rcs

# To enable verbose output run make with V=1
ifneq ($(V), 1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

%.a: $(objs)
	@echo $(deps)
	@echo "CC	$@"
	$(Q)$(ARCHIVE) $@ $^

%.o: %.c
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "CLEAN"
	$(Q)rm -rf $(lib) $(objs) $(deps)
