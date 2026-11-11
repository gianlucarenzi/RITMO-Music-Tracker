/* main.c - a song played by the RMT player in a C program (cc65), see README.md */
#include <conio.h>
#include <stdio.h>
#include "rmt.h"

/* The module: "Relocatable ASM for RMTPlayer" of RITMO, format CA65, start label _rmt_module (song.s) */
extern const unsigned char rmt_module[];

int main(void)
{
    unsigned char speed = rmt_init(rmt_module);
    rmt_vbi_on();

    clrscr();
    cprintf("RMT player in a cc65 program\r\n\r\n");
    cprintf("Instrument speed: %u\r\n", speed);
    cprintf("Any key stops the song.\r\n\r\n");
    while (!kbhit()) {
        unsigned int frames = rmt_frames;
        gotoxy(0, 5);
        cprintf("Frames played: %u   ", frames);
    }
    cgetc();
    rmt_vbi_off();
    return 0;
}
