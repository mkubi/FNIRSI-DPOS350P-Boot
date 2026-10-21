#include "image.h"

int image_is_egon_exec(const void *buffer, uint32_t length)
{
    static const char expected[8] = {'e','G','O','N','.','E','X','E'};
    const egon_exec_header_t *header = (const egon_exec_header_t *)buffer;
    unsigned int i;

    if (buffer == 0 || length < EGON_HEADER_MIN_SIZE)
        return 0;

    for (i = 0; i < sizeof(expected); i++) {
        if (header->magic[i] != expected[i])
            return 0;
    }

    if (header->length < EGON_HEADER_MIN_SIZE)
        return 0;

    return 1;
}
