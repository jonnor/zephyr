
EMLEARN_INCLUDE_DIR := /home/jon/projects/emlearn/emlearn

all: check

test_processing:
	gcc -o process tools/process.c -I ${EMLEARN_INCLUDE_DIR} -I./src/ -lm -g -fsanitize=address,undefined,leak -Wall -Werror -Wno-error=unused-function
	./process sensordata.csv features.csv 50

check: test_processing

.PHONY: test_processing

