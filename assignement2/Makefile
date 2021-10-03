CC=gcc 
FILES=main.c
TIMELIMIT=20:00
OPTIMIZATIONLEVEL=g3

HOST1=ee-tik-cn007
HOST2=ee-tik-cn008
HOST3=ee-tik-cn009

student: clean_student
	$(CC) -$(OPTIMIZATIONLEVEL) $(FILES) -lm -o 16-821-837 

student_run: clean_student student
	cp 16-821-837 /data/${USER}/16-821-837
# srun -t $(TIMELIMIT) /data/${USER}/main -t 100
	srun -t $(TIMELIMIT) -w $(HOST1) /data/${USER}/16-821-837 

main:
# compile the program on the head node
	$(CC) -$(OPTIMIZATIONLEVEL) $(FILES) -lm -o main

run: clean main	 
	cp main /data/${USER}/main
# srun -t $(TIMELIMIT) /data/${USER}/main -t 100
	srun -t $(TIMELIMIT) -w $(HOST1) /data/${USER}/main -m

run_clusters: clean main	
	cp main /data/${USER}/main

# run program on cluster node
	srun -t $(TIMELIMIT) -w $(HOST1) /data/${USER}/main > $(HOST1).csv
	srun -t $(TIMELIMIT) -w $(HOST2) /data/${USER}/main > $(HOST2).csv
	srun -t $(TIMELIMIT) -w $(HOST3) /data/${USER}/main > $(HOST3).csv

debug: clean main
	cp main /data/${USER}/main
	cp main.c /data/${USER}/main.c
# opens an interactive bash session on the remote node
	srun -t $(TIMELIMIT)-w $(HOST1) --pty bash -i

all: clean run
.PHONY: clean
clean:
	rm -rf main
clean_student:
	rm -rf 16-821-837  