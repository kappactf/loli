#include <string.h>

#include "loli_buffer_u16.h"
#include "loli_alloc.h"

loli_buffer_u16 *loli_new_buffer_u16(uint32_t start)
{
    loli_buffer_u16 *b = loli_malloc(sizeof(*b));
    b->data = loli_malloc(start * sizeof(*b->data));
    b->pos = 0;
    b->size = start;
    return b;
}

void loli_u16_write_1(loli_buffer_u16 *b, uint16_t one)
{
    if (b->pos + 1 > b->size) {
        b->size *= 2;
        b->data = loli_realloc(b->data, b->size * sizeof(*b->data));
    }

    b->data[b->pos] = one;
    b->pos++;
}

void loli_u16_write_2(loli_buffer_u16 *b, uint16_t one, uint16_t two)
{
    if (b->pos + 2 > b->size) {
        b->size *= 2;
        b->data = loli_realloc(b->data, b->size * sizeof(*b->data));
    }

    b->data[b->pos    ] = one;
    b->data[b->pos + 1] = two;
    b->pos += 2;
}

void loli_u16_write_3(loli_buffer_u16 *b, uint16_t one, uint16_t two,
        uint16_t three)
{
    if (b->pos + 3 > b->size) {
        b->size *= 2;
        b->data = loli_realloc(b->data, b->size * sizeof(*b->data));
    }

    b->data[b->pos    ] = one;
    b->data[b->pos + 1] = two;
    b->data[b->pos + 2] = three;
    b->pos += 3;
}

void loli_u16_write_4(loli_buffer_u16 *b, uint16_t one, uint16_t two,
        uint16_t three, uint16_t four)
{
    if (b->pos + 4 > b->size) {
        b->size *= 2;
        b->data = loli_realloc(b->data, b->size * sizeof(*b->data));
    }

    b->data[b->pos    ] = one;
    b->data[b->pos + 1] = two;
    b->data[b->pos + 2] = three;
    b->data[b->pos + 3] = four;
    b->pos += 4;
}

void loli_u16_write_5(loli_buffer_u16 *b, uint16_t one, uint16_t two,
        uint16_t three, uint16_t four, uint16_t five)
{
    if (b->pos + 5 > b->size) {
        b->size *= 2;
        b->data = loli_realloc(b->data, b->size * sizeof(*b->data));
    }

    b->data[b->pos    ] = one;
    b->data[b->pos + 1] = two;
    b->data[b->pos + 2] = three;
    b->data[b->pos + 3] = four;
    b->data[b->pos + 4] = five;
    b->pos += 5;
}

void loli_u16_write_6(loli_buffer_u16 *b, uint16_t one, uint16_t two,
        uint16_t three, uint16_t four, uint16_t five, uint16_t six)
{
    if (b->pos + 6 > b->size) {
        b->size *= 2;
        b->data = loli_realloc(b->data, b->size * sizeof(*b->data));
    }

    b->data[b->pos    ] = one;
    b->data[b->pos + 1] = two;
    b->data[b->pos + 2] = three;
    b->data[b->pos + 3] = four;
    b->data[b->pos + 4] = five;
    b->data[b->pos + 5] = six;
    b->pos += 6;
}

void loli_u16_write_prep(loli_buffer_u16 *b, uint32_t needed)
{
    if (b->pos + needed > b->size) {
        while ((b->pos + needed) > b->size)
            b->size *= 2;

        b->data = loli_realloc(b->data, sizeof(*b->data) * b->size);
    }
}

uint16_t loli_u16_pop(loli_buffer_u16 *b)
{
    uint16_t result = b->data[b->pos - 1];
    b->pos--;
    return result;
}

void loli_u16_inject(loli_buffer_u16 *b, int where, uint16_t value)
{
    if (b->pos + 1 > b->size) {
        b->size *= 2;
        b->data = loli_realloc(b->data, b->size * sizeof(*b->data));
    }

    int move_by = b->pos - where;

    memmove(b->data+where+1, b->data+where, move_by * sizeof(*b->data));
    b->pos++;
    b->data[where] = value;
}

void loli_free_buffer_u16(loli_buffer_u16 *b)
{
    loli_free(b->data);
    loli_free(b);
}
