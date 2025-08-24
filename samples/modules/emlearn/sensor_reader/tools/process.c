
// from emlearn, for CSV reader/writer
#include <eml_csv.h>
#include <eml_fileio.h>

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
#define OUTPUT_COLUMNS_LENGTH (accelgyro_features_length+1)
const char *output_columns[OUTPUT_COLUMNS_LENGTH] = {
    "time",
    "orientation_x",
    "orientation_y",
    "orientation_z",
    "acceleration_mag_rms"
};
float output_values[OUTPUT_COLUMNS_LENGTH];

// Working buffers
#define READ_BUFFER_SIZE 1024
char read_buffer[READ_BUFFER_SIZE];


int
main(int argc, const char *argv)
{
 
    // Setup file input and output
    FILE *read_file = fopen("sensordata.csv", "r");
    if (read_file == NULL) {
        fprintf(stderr, "failed to open input\n");
        return 1;
    }
    FILE *write_file = fopen("features.csv", "w");
    if (write_file == NULL) {
        fprintf(stderr, "failed to open output\n");
        return 1;
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



    for (int i=0; i<reader->n_columns; i++) {
        const bool correct = strcmp(input_columns[i], expect_columns[i]) == 0;
        if (!correct) {
            fprintf(stderr, "incorrect-sensordata-column index=%d got=%s expect=%s\n",
                i, input_columns[i], expect_columns[i]);
            return 1;
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
        const float *sensor_data = input_values+1; // first column is time, ignored
        accelgyro_preprocessor_run(&preprocessor, sensor_data, accelgyro_features_length);

        output_values[0] = 666.66f; // FIXME: set meaningful time value
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
