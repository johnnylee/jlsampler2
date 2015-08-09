
CC = gcc -Ofast -march=native -Wall -std=gnu11
LIBS = sndfile gtk+-3.0 jack alsa
PKGCONFIG = $(shell which pkg-config)
CFLAGS = $(shell $(PKGCONFIG) --cflags $(LIBS)) \
	-Wall -march=native -Ofast -fopenmp -pthread
LDFLAGS = $(shell $(PKGCONFIG) --libs $(LIBS)) \
	-Wall -march=native -Ofast -lgomp -pthread -lm

APP = jlsampler
SRC = main.c resources.c mem.c controls.c sample.c sampler.c ringbuffer.c \
	confconfig.c conftuning.c confcontrols.c rclowpass.c playingsample.c gui.c

OBJS = $(SRC:.c=.o)

all: $(APP)

%.o: %.c
	$(CC) -c -o $(@F) $(CFLAGS) $<

$(APP): $(OBJS)
	$(CC) -o $(@F) $(LDFLAGS) $(OBJS)

resources.c: gresource.xml gui.glade
	glib-compile-resources gresource.xml --target=resources.c --generate-source

format:
	indent \
		--linux-style \
		--no-tabs \
		--indent-level 4 \
		--line-length 79 \
		--comment-line-length 79 \
		*.h *.c
	rm *.h~ *.c~

clean:
	rm -f $(OBJS) $(APP) resources.c *~
