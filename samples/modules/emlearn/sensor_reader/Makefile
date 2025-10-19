
EMLEARN_INCLUDE_DIR := /home/jon/projects/emlearn/emlearn

CFLAGS := -g -fsanitize=address,undefined,leak -Wall -Werror -Wno-error=unused-function

all: check

test_processing:
	gcc -o preprocess tools/process.c -I ${EMLEARN_INCLUDE_DIR} -I./src/ -lm ${CFLAGS}
	./preprocess sensordata.csv features.csv 50

test_model:
	gcc -o process_model tools/process.c -I ${EMLEARN_INCLUDE_DIR} -I./src/ -lm -D MOTION_MODEL_FILE=\"motion_model.h\"
	./process_model sensordata.csv output.csv 50

check: test_processing test_model


.PHONY: test_processing

