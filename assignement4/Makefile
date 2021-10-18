CFLAGS += -O1 -mrtm
CFLAGS += -Iinclude
CFLAGS += -D_GNU_SOURCE=1
NODE=ee-tik-cn006
TIMELIMIT=20:00

LDFLAGS += -no-pie

#your student id goes here

STUDENTID=16-821-837

all: check $(STUDENTID)-meltdown_segv $(STUDENTID)-meltdown_tsx $(STUDENTID)-spectre

run: clean all
	cp ${STUDENTID}-meltdown_segv /data/${USER}/${STUDENTID}-meltdown_segv
	cp ${STUDENTID}-meltdown_tsx /data/${USER}/${STUDENTID}-meltdown_tsx
	cp ${STUDENTID}-spectre /data/${USER}/${STUDENTID}-spectre
	cp test.sh /data/${USER}/test.sh
	cp plot.sh /data/${USER}/plot.sh
	srun -t $(TIMELIMIT) -w $(NODE) bash -c '. /data/gthivolet/plot.sh'

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
