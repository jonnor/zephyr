

#define EML_CSV_VALUES_LENGTH 100
#define EML_CSV_BUFFER_LENGTH 1024

#include <eml_test.h> // from emlearn, for CSV reader
#include "eml_csv.h" // WIP, should go into emlearn. For CSV writer

#include "preprocessing.h"


// TODO: put somewhere generic
int eml_io_file_write(void *context, const uint8_t *buffer, size_t size)
{
    FILE* fptr = context;
    return fwrite(buffer, 1, size, fptr);
}

int eml_io_file_read(void *context, uint8_t *buffer, size_t size)
{
    FILE* fptr = context;
    return fread(buffer, 1, size, fptr);
}

int eml_io_file_seek(void *context, size_t position)
{
    FILE* fptr = context;
    return fseek(fptr, position, SEEK_SET);
}

#define N_DATA_COLUMNS 7
const char *columns[] = {
    "time",
    "acc_x",
    "acc_y",
    "acc_z",
    "gyro_x",
    "gyro_y",
    "gyro_z",
};

#define READ_BUFFER_SIZE 1024
char read_buffer[READ_BUFFER_SIZE];

int
main(int argc, const char *argv)
{
    
    // Write some simple file
    FILE *write_file = fopen("test.csv", "w");
    
    EmlCsvWriter _writer = {
        .n_columns = N_DATA_COLUMNS,
        .write = eml_io_file_write,
        .stream = write_file,
    };
    EmlCsvWriter *writer = &_writer;

    EmlError header_err = eml_csv_writer_write_header(writer, columns, N_DATA_COLUMNS);

    const float values[N_DATA_COLUMNS] = \
        { 0.0f, 1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f };

    EmlError write_err = eml_csv_writer_write_data(writer, values, N_DATA_COLUMNS);
    write_err = eml_csv_writer_write_data(writer, values, N_DATA_COLUMNS);

    printf("write-done header=%d write=%d \n", header_err, write_err);

    fclose(write_file);
    

    // Read back
    FILE *read_file = fopen("test.csv", "r");
    EmlCsvReader _reader = {
        .seek = eml_io_file_seek,
        .read = eml_io_file_read,
        .stream = read_file,
    };
    EmlCsvReader *reader = &_reader;

#define READ_COLUMNS_MAX 10
    char *read_columns[READ_COLUMNS_MAX];

    EmlError read_header_err = eml_csv_reader_read_header(reader,\
        read_buffer, READ_BUFFER_SIZE, read_columns, READ_COLUMNS_MAX);

    printf("header-status err=%d columms=%d\n", read_header_err, reader->n_columns);

    printf("columns: \n");
    for (int i=0; i<reader->n_columns; i++) {
        printf("%s\n", read_columns[i]);
    }

    EmlError data_err = eml_csv_reader_read_data(reader,\
        read_buffer, READ_BUFFER_SIZE, read_columns, READ_COLUMNS_MAX);
    printf("data-status err=%d\n", data_err);
    for (int i=0; i<reader->n_columns; i++) {
        printf("%s\n", read_columns[i]);
    }


    return 0;
}
