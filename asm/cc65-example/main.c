/* main.c - a song played by the RMT player in a C program (cc65), see README.md */
#include <conio.h>
#include <stdio.h>
#include "rmt.h"

/* The module: "Relocatable ASM for RMTPlayer" of RITMO, format CA65, start label _rmt_module (song.s) */
extern const unsigned char rmt_module[];

int main(void)
{
    unsigned char fade;
    unsigned char speed = rmt_init(rmt_module);
    rmt_vbi_on();

    clrscr();
    cprintf("RMT player in a cc65 program\r\n\r\n");
    cprintf("Instrument speed: %u\r\n", speed);
    cprintf("Any key fades the song out.\r\n\r\n");
    while (!kbhit()) {
        unsigned int frames = rmt_frames;
        gotoxy(0, 5);
        cprintf("Frames played: %u   ", frames);
    }
    cgetc();

    /* The fade: one volume step every 4 frames, then silence (the low byte of rmt_frames is enough, and is read
       in one access while the VBI counts) */
    for (fade = 1; fade < 16; ++fade) {
        unsigned char until = (unsigned char)rmt_frames + 4;
        rmt_global_volume_fade = fade << 4;
        while ((unsigned char)rmt_frames != until) {
        }
    }
    rmt_vbi_off();
    return 0;
}
