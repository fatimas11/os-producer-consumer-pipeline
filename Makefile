# Default target (what runs when you just type 'make')
all: ex3.out

# Link the object files to create the executable
ex3.out: main.o producer.o dispatcher.o coeditor.o screen_manager.o bounded_buffer.o buffered_open.o 
	gcc -pthread -o ex3.out main.o producer.o dispatcher.o coeditor.o screen_manager.o bounded_buffer.o buffered_open.o 

# Compile main.c
main.o: main.c System.h
	gcc -pthread -c main.c

# Compile producer.c
producer.o: producer.c System.h
	gcc -pthread -c producer.c

# Compile dispatcher.c
dispatcher.o: dispatcher.c System.h
	gcc -pthread -c dispatcher.c

# Compile coeditor.c
coeditor.o: coeditor.c System.h
	gcc -pthread -c coeditor.c

# Compile screen_manager.c
screen_manager.o: screen_manager.c System.h
	gcc -pthread -c screen_manager.c

# Compile bounded_buffer.c
bounded_buffer.o: bounded_buffer.c System.h
	gcc -pthread -c bounded_buffer.c

# Compile buffered_open.c
buffered_open.o: buffered_open.c buffered_open.h
	gcc -c buffered_open.c

# Clean up build files (run 'make clean')
clean:
	rm -f main.o producer.o dispatcher.o coeditor.o screen_manager.o bounded_buffer.o buffered_open.o ex3.out