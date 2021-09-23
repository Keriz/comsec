CC=gcc 
FILES=main.c
TIMELIMIT=20:00
HOST1=ee-tik-cn001
HOST2=ee-tik-cn002
HOST3=ee-tik-cn003

main:
# compile the program on the head node
	$(CC) -Werror $(FILES) -o main

run: clean main	
	cp main /data/${USER}/main

# run program on cluster node
    srun -t $(TIMELIMIT) /data/${USER}/main > output.csv


run_clusters: clean main	
	cp main /data/${USER}/main

# run program on cluster node
	srun -t $(TIMELIMIT) -w $(HOST1) /data/${USER}/main > $(HOST1).csv
	srun -t $(TIMELIMIT) -w $(HOST2) /data/${USER}/main > $(HOST2).csv
	srun -t $(TIMELIMIT) -w $(HOST3) /data/${USER}/main > $(HOST3).csv

debug: main
	cp main /data/${USER}/main
	cp dataplot.py /data/${USER}/dataplot.py
# opens an interactive bash session on the remote node
	srun -t $(TIMELIMIT) --pty bash -i

all: clean run
.PHONY: clean
clean:
	rm -rf main