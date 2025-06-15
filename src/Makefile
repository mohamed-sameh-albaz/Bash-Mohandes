CC = gcc
CFLAGS = -Wall -g

EXECS = os-sim process

all: $(EXECS)

clk.o: clk.c
	$(CC) $(CFLAGS) -c $< -o $@

scheduler.o: scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@

os-sim.o: process_generator.c
	$(CC) $(CFLAGS) -c $< -o $@

process.o: process.c
	$(CC) $(CFLAGS) -c $< -o $@

memory_manager.o: memory_manager.c
	$(CC) $(CFLAGS) -c $< -o $@

os-sim: clk.o scheduler.o os-sim.o memory_manager.o
	$(CC) -o $@ $^ -lm

process: scheduler.o clk.o process.o
	$(CC) -o $@ $^ -lm
	
clean:
	rm -f *.o $(EXECS)

.PHONY: all clean
