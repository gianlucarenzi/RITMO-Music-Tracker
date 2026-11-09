@echo off
if not exist out\ (mkdir out)
rem The target selectors are on the command line, not in rmt_feat.a65 (RMT 1.36, 5ae914a)
mads -d:FEAT_IS_TRACKER=1 -d:FEAT_IS_SIMPLEP=0 -hc:out\tracker_obx.h -l:out\tracker_obx.lst -o:out\tracker.obx rmtplayr.a65
rem mads -d:FEAT_IS_TRACKER=0 -d:FEAT_IS_SIMPLEP=1 -o:out\rmtplayer.xex rmtplayer.a65
pause

