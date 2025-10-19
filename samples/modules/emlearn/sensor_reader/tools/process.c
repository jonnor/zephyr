
// from emlearn, for CSV reader/writer
#include <eml_csv.h>
#include <eml_fileio.h>
#include <stdlib.h>
#include <errno.h>

#include "preprocessing.h"
#include "gravity_filter.h"

// Model
// -DMOTION_MODEL_FILE=\"motion_model_config\"
// NOTE: the model name should be "motion_model", so that motion_model_predict() works
#ifdef MOTION_MODEL_FILE
#include MOTION_MODEL_FILE
#endif
#define MOTION_MODEL_CLASSES 2

// Input format
#define SENSOR_DATA_COLUMNS 6

const char *expect_columns[7] = {
    "time",
    "acc_x",
    "acc_y",
    "acc_z",
    "gyro_x",
    "gyro_y",
    "gyro_z",
};
#define INPUT_COLUMNS_MAX 10
char *input_columns[INPUT_COLUMNS_MAX];
float input_values[INPUT_COLUMNS_MAX];

// Output format
// Add +1 because we also have time column
#define OUTPUT_COLUMNS_LENGTH (accelgyro_features_length+1+MOTION_MODEL_CLASSES)
// WARNING: length of items must match the number of features defined
const char *output_columns[OUTPUT_COLUMNS_LENGTH] = {
    "time",
    "orientation_x",
    "orientation_y",
    "orientation_z",
    "motion_mag_rms",
    "motion_mag_p2p",
    "motion_x_rms",
    "motion_y_rms",
    "motion_z_rms",
    "class_0",
    "class_1"
};
float output_values[OUTPUT_COLUMNS_LENGTH];


// Working buffers

// Read buffer for CSV row
// Can easily be 10x the number of columns, because of textual encoding of floating-point numbers
#define READ_BUFFER_SIZE 1000
char read_buffer[READ_BUFFER_SIZE];

// Sensor data window
#define WINDOW_LENGTH_MAX 100
float window_buffer[WINDOW_LENGTH_MAX*SENSOR_DATA_COLUMNS];


// WARN: uses errno, not reentrant/threadsafe
int parse_integer(const char *str, long *out)
{
    char *endptr;
    errno = 0;

    const long val = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0') {
        return -1;
    }

    *out = val;
    return 0;
}


// On success, returns the number of rows read. Normally @window_length, but might be shorter at end-of-input
// On error, returns a negative integer
int
read_overlapped_windows(EmlCsvReader *reader,
        int *read_index,
        int window_length,
        int hop_length,
        float *buffer,
        int buffer_length
    )
{
    const int out_columns = SENSOR_DATA_COLUMNS;

    // Check inputs
    if (buffer_length != window_length*out_columns) {
        //fprintf(stderr, "b=%d e=%d \n", buffer_length, window_length*out_columns);
        return -12;
    }

    if (reader->n_columns != out_columns+1) {
        fprintf(stderr, "b=%d e=%d \n", (int)reader->n_columns, out_columns+1);
        return -3;
    }

    // Check invariants
    if (*read_index > window_length) {
        // read_index should go from 0 to window_length
        return -2;
    }
    if (*read_index < 0) {
        return -2;
    }
    if (*read_index > (buffer_length/out_columns)) {
        return -2;
    }

    if (*read_index == window_length) {
        // previous execution hit a full window
        // prepare for new values coming in, by shifting existing data down
        memmove(buffer, buffer+(hop_length*out_columns), sizeof(float)*out_columns*window_length);
        *read_index -= hop_length;
    }

    // Check if we need
    const int max_rows = window_length;
    int row = -1;
    for (row=0; row<max_rows; row++) {
        const int values_read = eml_csv_reader_read_data(reader,\
            read_buffer, READ_BUFFER_SIZE, input_columns, INPUT_COLUMNS_MAX);
        if (values_read == 0) {
            // input stream finished, give partial window read
            return *read_index;
        }

        // Parse as numbers
        for (int i=0; i<reader->n_columns; i++) {
            const float v = strtod(input_columns[i], NULL);
            input_values[i] = v;
        }

        // NOTE: first column is time, ignored
        // TODO: generalize or move this logic elsewhere?
        const float *values = input_values+1;

        // Buffer received data
        const int read_offset = ((*read_index)*out_columns);
        memcpy(buffer+read_offset, values, sizeof(float)*out_columns);
        *read_index += 1;

        if (*read_index == window_length) {
            // A full window, signal
            return *read_index;
            // NOTE: rotating the buffer is done on next function call
        }

    }

    // still waiting for a full window
    return 0;
}



int
main(int argc, const char *argv[])
{
 
    if (argc < 4) {
        fprintf(stderr, "Expected 4+ arguments, got %d\n", argc);
        return -1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    long samplerate;
    const int samplerate_err = parse_integer(argv[3], &samplerate);
    if (samplerate_err != 0) {
        return -1;
    }

    const int window_length = 50; // FIXME: unhardcode
    const int hop_length = 8; // FIXME: unhardcode

    // Setup file input
    FILE *read_file = fopen(input_path, "r");
    if (read_file == NULL) {
        fprintf(stderr, "failed to open input\n");
        return -1;
    }
    EmlCsvReader _reader = {
        .seek = eml_fileio_seek,
        .read = eml_fileio_read,
        .stream = read_file,
    };
    EmlCsvReader *reader = &_reader;
    
    // Check input header is as expected
    const EmlError read_header_err = eml_csv_reader_read_header(reader,\
        read_buffer, READ_BUFFER_SIZE, input_columns, INPUT_COLUMNS_MAX);
    if (read_header_err != EmlOk) {
        return 2;
    }
    const int n_expect_columns = SENSOR_DATA_COLUMNS + 1; // sensor columns + time
    if (reader->n_columns != n_expect_columns) {
        return 2;
    }

    // Check that columns are as expected
    for (int i=0; i<reader->n_columns; i++) {
        const bool correct = strcmp(input_columns[i], expect_columns[i]) == 0;
        if (!correct) {
            fprintf(stderr, "incorrect-sensordata-column index=%d got=%s expect=%s\n",
                i, input_columns[i], expect_columns[i]);
            return 2;
        }
    }

    // Setup file output
    FILE *write_file = fopen(output_path, "w");
    if (write_file == NULL) {
        fprintf(stderr, "failed to open output\n");
        return -1;
    }

    EmlCsvWriter _writer = {
        .n_columns = OUTPUT_COLUMNS_LENGTH,
        .write = eml_fileio_write,
        .stream = write_file,
    };
    EmlCsvWriter *writer = &_writer;

    // Write output header
    const EmlError write_header_err = \
        eml_csv_writer_write_header(writer, output_columns, OUTPUT_COLUMNS_LENGTH);
    if (write_header_err != EmlOk) {
        fprintf(stderr, "header-write-fail error=%d \n", write_header_err);
        return -1;
    }

    if (window_length > WINDOW_LENGTH_MAX) {
        return -2;
    }
    const int window_buffer_length = window_length*SENSOR_DATA_COLUMNS;

    // Setup preprocessing
    struct accelgyro_preprocessor _preprocessor;
    struct accelgyro_preprocessor *preprocessor = &_preprocessor;

    const int init_err = accelgyro_preprocessor_init(preprocessor, window_length);
    if (init_err != 0) {
        fprintf(stderr, "preprocess init error %d\n", init_err);
        return -2;
    }

    const int gravity_err = accelgyro_preprocessor_set_gravity_lowpass(preprocessor,
        gravity_lowpass_values, gravity_lowpass_length);
    if (gravity_err != 0) {
        fprintf(stderr, "lowpass config error %d\n", gravity_err);
        return -2;
    }

    const int fft_config_err = accelgyro_preprocessor_set_fft_features(preprocessor, 1, 10);
    if (fft_config_err != 0) {
        fprintf(stderr, "FFT config error %d\n", fft_config_err);
        return -2;
    }

    // Setup model
    float model_predictions[MOTION_MODEL_CLASSES];
    for (int i=0; i<MOTION_MODEL_CLASSES; i++) {
        // default to negative values, indicating no-output
        model_predictions[i] = -1.0f;
    }

    // Processing loop
    int window_no = 0;
    int window_read_index = 0;
    while (1) {

        // Read input data
        const int window_read = read_overlapped_windows(reader, &window_read_index,
            window_length, hop_length,
            window_buffer, window_buffer_length);

        fprintf(stdout, "main-read-window window=%d index=%d \n", window_no, window_read_index);

        if (window_read == window_length) {
            // Complete window, process it
            window_no += 1;

            const int preprocess_err = \
                accelgyro_preprocessor_run(preprocessor, window_buffer, window_buffer_length);
            if (preprocess_err != 0) {
                fprintf(stderr, "preprocessor error %d\n", preprocess_err);
                return -3;
            }

            // Provide time as output
            const float hop_duration = hop_length / (float)samplerate;
            output_values[0] = window_no * hop_duration;

            // Provide extracted features as output
            for (int i=0; i<accelgyro_features_length; i++) {
                output_values[i+1] = preprocessor->features[i];
            }

            // Run through classifier (if enabled)
#ifdef MOTION_MODEL_FILE
            int model_err = motion_model_predict_proba(preprocessor->features, accelgyro_features_length, model_predictions, MOTION_MODEL_CLASSES);
            if (model_err != 0) {
                fprintf(stderr, "failed to run model %d\n", model_err);
                return -4;
            }
#endif

            // Provide model predictions as output
            for (int i=0; i<MOTION_MODEL_CLASSES; i++) {
                output_values[i+1+accelgyro_features_length] = model_predictions[i];
            }

            // Write output values to file
            const EmlError write_err = \
                eml_csv_writer_write_data(writer, output_values, OUTPUT_COLUMNS_LENGTH);
            if (write_err != EmlOk) {
                fprintf(stderr, "failed to write output\n");
                return -3;
            }

        } else if (window_read < 0) {
            // error
            fprintf(stderr, "window read error %d\n", window_read);
            break;
        } else if (window_read != 0) {
            // partial window, typically at end-of-input. Ignored for now
            break;
        } else {
            // incomplete window, do nothing - wait for it to become complete
        }

    }

    fclose(write_file);
    fclose(read_file);

    const float last_timestamp = output_values[0];
    printf("main-done windows=%d time=%.3f\n", window_no, last_timestamp);

    return 0;
}
