
EMLEARN_INCLUDE_DIR := /home/jon/projects/emlearn/emlearn

#CFLAGS := -g 
CFLAGS := -g -fsanitize=address,undefined,leak -Wall -Werror -Wno-error=unused-function

all: check

src/gravity_filter.h:
	python gravity_lowpass.py

src/motion_model.h:
	python convert_model.py

test_processing: src/gravity_filter.h
	gcc -o preprocess tools/process.c -I ${EMLEARN_INCLUDE_DIR} -I./src/ -lm ${CFLAGS}
	./preprocess sensordata.csv features.csv 50

test_model: src/motion_model.h src/gravity_filter.h
	gcc -o process_model tools/process.c -I ${EMLEARN_INCLUDE_DIR} -I./src/ -lm ${CFLAGS} -DMOTION_MODEL_FILE=\"motion_model.h\"
	./process_model sensordata.csv output.csv 50

check: test_processing test_model


.PHONY: test_processing

