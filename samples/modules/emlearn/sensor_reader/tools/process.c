
// from emlearn, for CSV reader/writer
#include <eml_csv.h>
#include <eml_fileio.h>
#include <stdlib.h>
#include <errno.h>

#include "preprocessing.h"

// Input format
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
// WARNING: length must match
#define OUTPUT_COLUMNS_LENGTH (accelgyro_features_length)
const char *output_columns[OUTPUT_COLUMNS_LENGTH] = {
    "time",
    "orientation_x",
    "orientation_y",
    "orientation_z",
    "motion_mag_rms"
    "motion_mag_p2p",
    "motion_x_rms",
    "motion_y_rms",
    "motion_z_rms"
};
float output_values[OUTPUT_COLUMNS_LENGTH];


// Working buffers
#define READ_BUFFER_SIZE 1024
char read_buffer[READ_BUFFER_SIZE];


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

    // Setup file input and output
    FILE *read_file = fopen(input_path, "r");
    if (read_file == NULL) {
        fprintf(stderr, "failed to open input\n");
        return -1;
    }
    FILE *write_file = fopen(output_path, "w");
    if (write_file == NULL) {
        fprintf(stderr, "failed to open output\n");
        return -1;
    }

    EmlCsvReader _reader = {
        .seek = eml_fileio_seek,
        .read = eml_fileio_read,
        .stream = read_file,
    };
    EmlCsvReader *reader = &_reader;
    
    EmlCsvWriter _writer = {
        .n_columns = OUTPUT_COLUMNS_LENGTH,
        .write = eml_fileio_write,
        .stream = write_file,
    };
    EmlCsvWriter *writer = &_writer;

    const EmlError write_header_err = \
        eml_csv_writer_write_header(writer, output_columns, OUTPUT_COLUMNS_LENGTH);
    if (write_header_err != EmlOk) {
        fprintf(stderr, "header-write-fail error=%d \n", write_header_err);
        return 1;
    }

    // Check input header is as expected
    const EmlError read_header_err = eml_csv_reader_read_header(reader,\
        read_buffer, READ_BUFFER_SIZE, input_columns, INPUT_COLUMNS_MAX);
    if (read_header_err != EmlOk) {
        return 2;
    }
    const int n_expect_columns = 7;
    if (reader->n_columns != n_expect_columns) {
        return 2;
    }

    for (int i=0; i<reader->n_columns; i++) {
        const bool correct = strcmp(input_columns[i], expect_columns[i]) == 0;
        if (!correct) {
            fprintf(stderr, "incorrect-sensordata-column index=%d got=%s expect=%s\n",
                i, input_columns[i], expect_columns[i]);
            return 2;
        }
    }

    // Setup preprocessing
    struct accelgyro_preprocessor preprocessor;

    // Read and process data
    const int max_rows = 10000;
    int row = -1;
    for (row=0; row<max_rows; row++) {
        const int values_read = eml_csv_reader_read_data(reader,\
            read_buffer, READ_BUFFER_SIZE, input_columns, INPUT_COLUMNS_MAX);
        if (values_read == 0) {
            // finished
            break;
        }

        // Parse as numbers
        for (int i=0; i<reader->n_columns; i++) {
            const float v = strtod(input_columns[i], NULL);
            input_values[i] = v;
        }

        // FIXME: need to batch up N rows of sensor data
        // Run through preprocessor
        const int hop_length = 1;
        const float *sensor_data = input_values+1; // first column is time, ignored
        const int window_samples_total = hop_length * 6;

        accelgyro_preprocessor_run(&preprocessor, sensor_data, window_samples_total);

        // FIXME: compute output time based on the window
        output_values[0] = input_values[0];
        for (int i=0; i<accelgyro_features_length; i++) {
            output_values[i+1] = preprocessor.features[i];
        }

        // Write output values
        const EmlError write_err = \
            eml_csv_writer_write_data(writer, output_values, OUTPUT_COLUMNS_LENGTH);
        if (write_err != EmlOk) {
            fprintf(stderr, "failed to write output\n");
            return 2;
        }
    }

    fclose(write_file);
    fclose(read_file);

    printf("main-done rows=%d \n", row);

    return 0;
}
