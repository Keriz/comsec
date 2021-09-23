CC=gcc 
FILES=main.c
TIMELIMIT=20:00

main:
# compile the program on the head node
	$(CC) -Werror $(FILES) -o main

run: main	
	cp main /data/${USER}/main
# run program on cluster node
	srun -t $(TIMELIMIT) --pty /data/${USER}/main

debug: main
	cp main /data/${USER}/main
	cp dataplot.py /data/${USER}/dataplot.py
# opens an interactive bash session on the remote node
	srun -t $(TIMELIMIT) --pty bash -i

all: clean run
.PHONY: clean
clean:
	rm -rf main