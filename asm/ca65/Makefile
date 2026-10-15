# Makefile - the RMT Atari binaries from the ca65 sources (cc65 instead of MADS/XASM), see README.md
#
#   make            the drivers and players into build/
#   make players    the Simple RMT Player (XEX, SAP) and the RMT Visualizer with asm/Patch-16/music.rmt
#   make check      also compare the drivers with those RMT uses (rmt/resources/drivers) and the C headers with
#                   those of src/ (tracker_obx.h, legacy_obx.h)
#   make headers    the C headers of the drivers into build/, as MADS -hc writes them (obxheader.py)
#   make install-headers  copy them to src/ (formatted with clang-format, when it is there)
#   make sources    translate the MADS sources of asm/ again (scripts/mads2ca65.py)
#   make clean

OBX     = ./obx.sh
BUILD   = build
DRIVERS = ../../rmt/resources/drivers
SRC     = ../../src
HEADERS = $(BUILD)/tracker_obx.h $(BUILD)/legacy_obx.h

# Patch16 RMT driver (tracker.obx = rmt_driver_v6.obx): asm/Patch-16/rmtplayr.a65 with rmt_feat.a65 as it is
# RMT 1.28 driver (rmt_ata.sys = rmt_driver_v1.obx): asm/Legacy/rmt_ata.a65
# RMT 1.28 SAP players for 4 and 8 tracks, XEX export routine: asm/Legacy/rmt_sap4/8.a65, rmt_msx.a65
# RMF player with its test song: asm/Legacy/rmf/rmf_play.a65
TARGETS = $(BUILD)/rmt_driver_v6.obx $(BUILD)/rmt_driver_v1.obx \
          $(BUILD)/rmt_sap4.obx $(BUILD)/rmt_sap8.obx $(BUILD)/rmt_msx.obx \
          $(BUILD)/rmf_play.xex

# Simple RMT Player (XEX or SAP with music.rmt) and RMT Visualizer (XEX with music.rmt) of Patch-16: the player
# routine as FEAT_IS_SIMPLEP (at $3100)
PLAYERS = $(BUILD)/rmtplayer.xex $(BUILD)/rmtplayer.sap $(BUILD)/rmtvis.xex

PATCH16 = $(wildcard Patch-16/*.s)
LEGACY  = $(wildcard Legacy/*.s)
RMF     = $(wildcard Legacy/rmf/*.s)
COMMON  = mads.inc obx.cfg obx.sh

all: $(TARGETS)

players: $(PLAYERS)

$(BUILD):
	mkdir -p $@

# the drivers also write the ld65 debug file (-g does not change the binary), the symbols of the C headers
$(BUILD)/rmt_driver_v6.obx: $(PATCH16) $(COMMON) | $(BUILD)
	OBX_DBG=$(BUILD)/rmt_driver_v6.dbg $(OBX) $@ Patch-16/rmtplayr.s

$(BUILD)/rmt_driver_v1.obx: $(LEGACY) $(COMMON) | $(BUILD)
	OBX_DBG=$(BUILD)/rmt_driver_v1.dbg $(OBX) $@ Legacy/rmt_ata.s

# the C headers the tracker addresses the variables of the drivers through: what MADS writes with -hc for
# asm/Patch-16/rmtplayr.a65 (prefix RMTPLAYR) and asm/Legacy/rmt_ata.a65 (prefix RMT_ATA)
$(BUILD)/tracker_obx.h: $(BUILD)/rmt_driver_v6.obx obxheader.py
	python3 obxheader.py $(BUILD)/rmt_driver_v6.dbg RMTPLAYR $@

$(BUILD)/legacy_obx.h: $(BUILD)/rmt_driver_v1.obx obxheader.py
	python3 obxheader.py $(BUILD)/rmt_driver_v1.dbg RMT_ATA $@

headers: $(HEADERS)

install-headers: $(HEADERS)
	cp $(HEADERS) $(SRC)/
	if command -v clang-format >/dev/null; then cd $(SRC) && clang-format -i tracker_obx.h legacy_obx.h; fi

$(BUILD)/rmt_sap4.obx $(BUILD)/rmt_sap8.obx $(BUILD)/rmt_msx.obx: $(BUILD)/%.obx: $(LEGACY) $(COMMON) | $(BUILD)
	$(OBX) $@ Legacy/$*.s

$(BUILD)/rmf_play.xex: $(RMF) $(COMMON) | $(BUILD)
	$(OBX) $@ Legacy/rmf/rmf_play.s

$(BUILD)/rmtplayer.xex: $(PATCH16) $(COMMON) | $(BUILD)
	$(OBX) $@ Patch-16/rmtplayer.s -D FEAT_IS_TRACKER=0 -D FEAT_IS_SIMPLEP=1

$(BUILD)/rmtplayer.sap: $(PATCH16) $(COMMON) | $(BUILD)
	$(OBX) $@ Patch-16/rmtplayer.s -D FEAT_IS_TRACKER=0 -D FEAT_IS_SIMPLEP=1 -D EXPORTXEX=0 -D EXPORTSAP=1

$(BUILD)/rmtvis.xex: $(PATCH16) $(COMMON) | $(BUILD)
	$(OBX) $@ Patch-16/rmtvis.s -D FEAT_IS_TRACKER=0 -D FEAT_IS_SIMPLEP=1

# the headers of src/ are compared without the alignment clang-format gives them
check: $(BUILD)/rmt_driver_v6.obx $(BUILD)/rmt_driver_v1.obx $(HEADERS)
	cmp $(BUILD)/rmt_driver_v6.obx $(DRIVERS)/rmt_driver_v6.obx
	cmp $(BUILD)/rmt_driver_v1.obx $(DRIVERS)/rmt_driver_v1.obx
	@echo "check: the drivers are identical to rmt_driver_v6.obx and rmt_driver_v1.obx"
	@for h in $(notdir $(HEADERS)); do \
	    awk '{ $$1 = $$1 } 1' $(SRC)/$$h | cmp -s - $(BUILD)/$$h || \
	        { echo "check: src/$$h is not what the driver source gives (make install-headers)" >&2; exit 1; }; \
	done
	@echo "check: src/tracker_obx.h and src/legacy_obx.h are those of the driver sources"

sources:
	cd ../.. && python3 scripts/mads2ca65.py asm asm/ca65

clean:
	rm -rf $(BUILD)

.PHONY: all players check headers install-headers sources clean
