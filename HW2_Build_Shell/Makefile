.PHONY: all
all: task1 task2 task3

task1: shell1.c
	gcc shell1.c -o shell1

task2: shell1.c
	gcc shell2.c -o shell2

task3: shell1.c
	gcc shell3.c -o shell3


.PHONY: clean
clean:
	-rm shell1 shell2 shell3 2>/dev/null
