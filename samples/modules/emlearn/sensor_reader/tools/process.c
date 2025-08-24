

#define EML_CSV_VALUES_LENGTH 100
#define EML_CSV_BUFFER_LENGTH 1024

#include <eml_test.h> // from emlearn, for CSV reader
#include "eml_csv.h" // WIP, should go into emlearn. For CSV writer

#include "preprocessing.h"


void row_callback(const float *values, int length, int row)
{

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

typedef int (*EmlCsvWriteFunction)(void *context, const uint8_t *buffer, size_t size);


int file_write(void *context, const uint8_t *buffer, size_t size)
{
    FILE* fptr = context;
    return fwrite(buffer, 1, size, fptr);
}


int
main(int argc, const char *argv)
{

    //FILE *fp = fopen("");
    //eml_test_read_csv();

    FILE *write_file = fopen("test.csv", "w");
    
    EmlCsvWriter _writer = {
        .n_columns = N_DATA_COLUMNS,
        .write = file_write,
        .stream = write_file,
    };
    EmlCsvWriter *writer = &_writer;


    EmlError header_err = eml_csv_writer_write_header(writer, columns, N_DATA_COLUMNS);

    const float values[N_DATA_COLUMNS] = \
        { 0.0f, 1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f };

    EmlError write_err = eml_csv_writer_write_data(writer, values, N_DATA_COLUMNS);

    printf("main-done header=%d write=%d \n", header_err, write_err);

    return 0;
}
