OUTFILE = ctrl
HOSTNAME = localhost
PORT = 4711
ELEVATORS = 3

run : $(OUTFILE)
	./$(OUTFILE) $(ELEVATORS) $(HOSTNAME) $(PORT)
	
all : $(OUTFILE)

$(OUTFILE) : ctrl.c hardwareAPI.o queue.o 
	gcc -g -o $(OUTFILE) $^ -Wextra -Wall -fsanitize=address

hardwareAPI.o : hardwareAPI.c
	gcc -g -c $^ -Wextra -Wall -fsanitize=address

queue.o : queue.c
	gcc -g -c $^ -Wextra -Wall -fsanitize=address