CFLAGS += -O3 -mrtm
CFLAGS += -Iinclude
CFLAGS += -D_GNU_SOURCE=1

LDFLAGS += -no-pie

#your student id goes here

STUDENTID=99-999-999

all: check $(STUDENTID)-meltdown_segv $(STUDENTID)-meltdown_tsx $(STUDENTID)-spectre

check:
ifndef STUDENTID
		$(error STUDENTID is undefined. Please specify it in the Makefile.)
endif

$(STUDENTID)-meltdown_segv: main-meltdown_segv.o
	${CC} $< -o $@ ${LDFLAGS} ${CFLAGS} ${LIBS}

$(STUDENTID)-meltdown_tsx: main-meltdown_tsx.o
	${CC} $< -o $@ ${LDFLAGS} ${CFLAGS} ${LIBS}

$(STUDENTID)-spectre: main-spectre.o
	${CC} $< -o $@ ${LDFLAGS} ${CFLAGS} ${LIBS}

%.o: %.c
	${CC} -c $< -o $@ ${CFLAGS} -MT $@ -MMD -MP -MF $(@:.o=.d)

clean: check
	rm -f $(STUDENTID)-* *.o *.d
