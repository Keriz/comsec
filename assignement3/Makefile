BUILD ?= obj

CFLAGS += -Iinclude -DHARDENED -DEVICT

STUDENTID=16-821-837

NODE=ee-tik-cn007
TIMELIMIT=20:00


all: libkrypto.so ${STUDENTID}
.PHONY: clean

-include source/Makefile

krypto-obj = $(addprefix ${BUILD}/, ${krypto-obj-y})
${STUDENTID}-obj = $(addprefix ${BUILD}/, ${${STUDENTID}-obj-y})

deps += $(krypto-obj:.o=.d)
deps += $(${STUDENTID}-obj:.o=.d)

-include ${deps}

libkrypto.so: CFLAGS += -fPIC
libkrypto.so: LDFLAGS += -shared
libkrypto.so: $(krypto-obj)
#@echo "LD $(notdir $@)"
	@mkdir -p "$(dir $@)"
	@${CC} ${krypto-obj} -o $@ ${LDFLAGS} ${CFLAGS} ${LIBS}

${STUDENTID}: libkrypto.so ${${STUDENTID}-obj}
#@echo "LD $(notdir $@)"
	@mkdir -p "$(dir $@)"
	@${CC} ${${STUDENTID}-obj} -o $@ ${LDFLAGS} ${CFLAGS} ${LIBS} -L. -lkrypto

$(BUILD)/%.o: %.c
#@echo "CC $<"
	@mkdir -p "$(dir $@)"
	@${CC} -c $< -o $@ ${CFLAGS} -MT $@ -MMD -MP -MF $(@:.o=.d)

run: clean all
	cp ${STUDENTID} /data/${USER}/${STUDENTID}
	cp libkrypto.so /data/${USER}/libkrypto.so
	srun -t $(TIMELIMIT) -w $(NODE) bash -c 'LD_LIBRARY_PATH="$$LD_LIBRARY_PATH:/data/${USER}/" /data/${USER}/${STUDENTID}'
clean:
	@rm -f ${STUDENTID} libkrypto.so
	@rm -rf ${BUILD}
