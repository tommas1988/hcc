CC = gcc

ROOT = ..
CFLAGS = -Wall -I$(ROOT)/deps/inih
FILES = hcc.c error.c hash.c sq_list.c $(ROOT)/deps/inih/ini.c

HCC = $(ROOT)/out/hcc

ifeq ($(DEBUG), yes)
	CFLAGS += -g -ggdb -DDEBUG

ifdef BUFFER_SIZE
	CFLAGS += -DBUFFER_SIZE=$(BUFFER_SIZE)
endif

endif

all: $(HCC)
	@printf 'Run with DEBUG = %s\n' $(DEBUG)

.PHONY: all

$(HCC): $(FILES) comment_defs_string.c $(patsubst %.c, %.h, $(FILES))
	$(CC) -o $@ $(CFLAGS) $(FILES)

comment_defs_string.c: default_comment_defs.ini build_comment_defs_string.sh
	sh build_comment_defs_string.sh

clean:
	-rm $(HCC) comment_defs_string.c > /dev/null 2>&1

.PHONY: clean


