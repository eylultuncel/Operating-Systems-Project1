all: main my_mpirun

main: main.c
	gcc -o main main.c -lpthread -lrt

my_mpirun: my_mpirun.c
	gcc -o my_mpirun my_mpirun.c -lpthread 



