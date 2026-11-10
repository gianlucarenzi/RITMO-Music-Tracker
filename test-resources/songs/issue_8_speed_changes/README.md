File Names:     swing.rmt, swing_two_channels.rmt
Author:         made for RITMO (2026-11-10), with the instrument 00 of Delta
Source:         https://github.com/raster-atari-org/RASTER-Music-Tracker/issues/8
                ("Unstable speed when using speed changes command in patterns",
                reported without a module)

swing.rmt: mono (RMT4), track length 16, song speed 5, instrument speed 1.
Track 00 sets the speed on every line, 3 and 7 alternating (a "swing").

swing_two_channels.rmt: the same track 00 on channel 1, and on channel 2 a
track that sets speed 5 on every fourth line; instrument speed 2. On those
lines two channels set a speed: the last channel wins.

"RmtCoreTest --swing <module>" plays 50 track lines twice: with the tracker
(CSong::PlayVBI(), which counts the frames of a line) and with the Patch16
player on its own (the module at $4000, rmt_init, then rmt_play instrument
speed times per frame, as an exported XEX does). Every line must last the
frames of its speed, in both. It does (RITMO 2.7): the issue does not
reproduce in RITMO.
