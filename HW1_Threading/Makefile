.PHONY: all
all: task

task: codec.h main.c
	gcc main.c codec.c -o coder -pthread

.PHONY: clean
clean:
	-rm coder 2>/dev/null
