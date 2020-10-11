#ifndef __VBE_H__
#define __VBE_H__

#include <stdint.h>
#include <startup/stivale.h>

extern int vbe_available;

extern uint32_t *vbe_framebuffer;
extern int vbe_width;
extern int vbe_height;
extern int vbe_pitch;

void init_vbe(struct stivale_framebuffer_t *);

#endif
