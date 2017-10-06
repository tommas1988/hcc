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

$(HCC): $(FILES) comment_def_config.c $(patsubst %.c, %.h, $(FILES))
	$(CC) -o $@ $(CFLAGS) $(FILES)

comment_def_config.c: comment_def_config.ini build_comment_def_config.sh
	sh build_comment_def_config.sh

clean:
	-rm $(HCC) comment_def_config.c > /dev/null 2>&1

.PHONY: clean


