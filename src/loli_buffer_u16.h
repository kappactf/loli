#ifndef LOLI_BUFFER_U16_H
# define LOLI_BUFFER_U16_H

# include <inttypes.h>

typedef struct {
    uint16_t *data;
    uint32_t pos;
    uint32_t size;
} loli_buffer_u16;

loli_buffer_u16 *loli_new_buffer_u16(uint32_t);

void loli_u16_write_1(loli_buffer_u16 *, uint16_t);
void loli_u16_write_2(loli_buffer_u16 *, uint16_t, uint16_t);
void loli_u16_write_3(loli_buffer_u16 *, uint16_t, uint16_t, uint16_t);
void loli_u16_write_4(loli_buffer_u16 *, uint16_t, uint16_t, uint16_t, uint16_t);
void loli_u16_write_5(loli_buffer_u16 *, uint16_t, uint16_t, uint16_t, uint16_t,
        uint16_t);
void loli_u16_write_6(loli_buffer_u16 *, uint16_t, uint16_t, uint16_t, uint16_t,
        uint16_t, uint16_t);

void loli_u16_write_prep(loli_buffer_u16 *, uint32_t);

uint16_t loli_u16_pop(loli_buffer_u16 *);

#define loli_u16_pos(b) b->pos
#define loli_u16_get(b, pos) b->data[pos]
#define loli_u16_set_pos(b, what) b->pos = what
#define loli_u16_set_at(b, where, what) b->data[where] = what
void loli_u16_inject(loli_buffer_u16 *, int, uint16_t);

void loli_free_buffer_u16(loli_buffer_u16 *);

#endif
