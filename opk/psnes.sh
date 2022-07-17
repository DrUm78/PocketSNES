#!/bin/sh
# Copy PocketSNES binary in /usr/games if md5 is different
if [ `md5sum /usr/games/psnes | cut -d' ' -f1` != `md5sum psnes | cut -d' ' -f1` ]; then
	rw
	cp -f psnes /usr/games
	ro
fi
exec /usr/games/launchers/snes_launch.sh "$1"
