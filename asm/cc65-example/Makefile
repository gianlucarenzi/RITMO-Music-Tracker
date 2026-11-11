# Makefile - a song played by the RMT player in a C program (cc65), see README.md
#
#   make              build/example.xex: the program, the module of SONG and the player, in one Atari executable
#   make SONG=x.rmt   with another song (an 8-track song needs STEREO=1)
#   make clean
#
# Needs cc65 (cl65, ca65, ld65), and RITMO (build-qt/out/ritmo, or RITMO=...) to export the song.

RITMO   ?= ../../build-qt/out/ritmo
SONG    ?= ../../rmt/songs/rmt128/raster/gemx.rmt
STEREO  ?= 0
BUILD   = build
OBX     = ../ca65/obx.sh

all: $(BUILD)/example.xex

$(BUILD):
	mkdir -p $@

# The module of the song as ca65 source: "Relocatable ASM for RMTPlayer", format CA65, no relocation (the linker
# places it), start label _rmt_module (the C name rmt_module); the exported FEAT_ section is not used here: the
# player has every feature
$(BUILD)/song.s: $(SONG) | $(BUILD)
	printf 'set output "%s"\nset overwrite yes\nopen "%s"\nexport rmtplayer-asm song.asm relocate=none asmformat=ca65 startlabel=_rmt_module\n' \
		"$(abspath $(BUILD))" "$(abspath $(SONG))" >$(BUILD)/song.rmtscript
	XDG_CONFIG_HOME=$(abspath $(BUILD))/config "$(RITMO)" "/SCRIPT:$(abspath $(BUILD))/song.rmtscript"
	mv $(BUILD)/song.asm $@

# The Patch16 player at $3100 (FEAT_IS_SIMPLEP), its frequency tables at $3A00; player.inc has its address
$(BUILD)/player.obx: $(wildcard ../ca65/Patch-16/*.s) | $(BUILD)
	OBX_SYMBOLS=RASTERMUSICTRACKER $(OBX) $@ ../ca65/Patch-16/rmtplayr.s \
		-D FEAT_IS_TRACKER=0 -D FEAT_IS_SIMPLEP=1 -D EXPORTXEX=0 -D EXPORTSAP=0 -D TABLES=\$$3A00 \
		-D STEREOMODE=$(STEREO) >$(BUILD)/player.sym
	awk '$$1 == "OBX_SYMBOL" { print "RMT_PLAYER = $$" $$3 }' $(BUILD)/player.sym >$(BUILD)/player.inc

# The program with the module, then the player: one XEX (the player loads before the program runs)
$(BUILD)/example.xex: main.c rmt.s rmt.h atari-rmt.cfg $(BUILD)/song.s $(BUILD)/player.obx
	cl65 -t atari -Osir -C atari-rmt.cfg --asm-include-dir $(BUILD) -m $(BUILD)/example.map \
		-o $(BUILD)/program.xex main.c rmt.s $(BUILD)/song.s
	rm -f main.o rmt.o $(BUILD)/song.o
	cat $(BUILD)/program.xex $(BUILD)/player.obx >$@

clean:
	rm -rf $(BUILD) main.o rmt.o

.PHONY: all clean
