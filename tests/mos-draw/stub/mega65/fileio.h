/* Stub <mega65/fileio.h> for the mos-draw harness (see ../mega65.h). */
#ifndef MOSDRAW_STUB_FILEIO_H
#define MOSDRAW_STUB_FILEIO_H

#include <stdint.h>

uint8_t open(char *name);
void close(uint8_t fd);
uint16_t read512(uint8_t *buf);

#endif
