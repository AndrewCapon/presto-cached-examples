#include "af-file-io.h"
#include "ff.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>

void* fileio_open(const char* filename) {
    printf("IMPL: Opening %s\n", filename);
    FIL *fil = (FIL*)malloc(sizeof(FIL));
    FRESULT fr = f_open(fil, filename, FA_READ);
    if(fr == FR_OK) {
        return (void *)fil;
    } else {
        free(fil);
        return NULL;
    }
}

void fileio_close(void* fhandle) {
    f_close((FIL*)fhandle);
    free(fhandle);
}

size_t fileio_read(void* fhandle, void *buf, size_t len) {
    unsigned int bytes_read;
    FRESULT fr = f_read((FIL*)fhandle, buf, len, &bytes_read);
    return fr == FR_OK ? bytes_read : 0;
}

int fileio_getc(void* fhandle) {
    unsigned char buf;
    (void *)fileio_read(fhandle, (void*)&buf, (size_t)1);
    return (int)buf;
}

size_t fileio_tell(void* fhandle) {
    return f_tell((FIL*)fhandle);
}

size_t fileio_seek(void* fhandle, size_t pos) {
    f_lseek((FIL*)fhandle, pos);
    return f_tell((FIL*)fhandle);
}