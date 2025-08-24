
// Basic support for Comma Separated Values (CSV)
// As per RFC4180, https://www.ietf.org/rfc/rfc4180.txt
// Not intended to parse every wierd "csv" like thing out there
// Just the well-formed thing, that can be produced from typical PyData etc tools

#include <eml_common.h>

#include <stdint.h>

// I/O functions
// FIXME: standardize and share the stream definitions with eml_csv.h

typedef int (*EmlCsvReadFunction)(void *context, uint8_t *buffer, size_t size);
typedef int (*EmlCsvSeekFunction)(void *context, size_t position);
typedef int (*EmlCsvWriteFunction)(void *context, const uint8_t *buffer, size_t size);

#define EML_CSV_DELIMITER ","
#define EML_CSV_EOL "\n"

typedef struct _EmlCsvReader {

    size_t n_columns;

    // IO
    EmlCsvReadFunction read;
    void *stream;
} EmlCsvReader;


typedef struct _EmlCsvWriter {

    size_t n_columns;

    // IO
    EmlCsvWriteFunction write;
    void *stream;
} EmlCsvWriter;


// Writer
EmlError
eml_csv_writer_write_header(EmlCsvWriter *self, const char **columns, size_t n_columns)
{

    // Keep number of expected values per row
    self->n_columns = n_columns;
    const int data_length = n_columns;

    const char *delimiter = EML_CSV_DELIMITER;
    const char *endline = EML_CSV_EOL;

    char buf[10] = {0,};
    for (int i=0; i<data_length; i++) {
        const char *name = columns[i];
        const int length = strlen(name);
        self->write(self->stream, name, length);
        const bool is_last = (i == data_length-1);
        if (!is_last) {
            self->write(self->stream, delimiter, 1);
        }
    }

    self->write(self->stream, endline, 1);

    return EmlOk;
}

EmlError
eml_csv_writer_write_data(EmlCsvWriter *self, const float *data, size_t data_length)
{
    EML_PRECONDITION(self->n_columns == data_length, EmlSizeMismatch);

    const char *endline = EML_CSV_EOL;

    char buf[10] = {0,};
    for (int i=0; i<data_length; i++) {
        int written = snprintf(buf, 10, "%f", data[i]);
        if (i == data_length-1) {
            buf[written] = (EML_CSV_DELIMITER)[0];
            written += 1;
        }
        self->write(self->stream, buf, written);
    }
    self->write(self->stream, endline, 1);

    return EmlOk;
}
