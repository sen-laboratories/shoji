#!/bin/sh
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 SEN Labs e.U.
#
# The template of a place in Shoji: the window opens with the form and the map, and the tiles of the map reach the cache. Run it in the Haiku VM with Shoji installed (the package or bin/shoji of
# the build: SHOJI=<path>), with a network connection for the tiles. Prints `ok` or `FAIL` for each check, `FAILED` at the end, and leaves the screenshot place-template.png in TEST_DIR to look at
# (the map with the pin, the name and type fields, the picture on the stripe).
SHOJI=${SHOJI:-/boot/system/apps/Shoji}
T=${TEST_DIR:-/Develop/test}/_shoji-place
CACHE=$HOME/config/cache/shoji/tiles
rm -rf "$T"; mkdir -p "$T"; cd "$T" || exit 1
failed=0
check() { desc=$1; shift; if "$@" >/dev/null 2>&1; then echo "ok   $desc"; else echo "FAIL $desc"; failed=1; fi; }

( while true; do kill -9 $(ps | grep "[s]creen_blanker" | awk '{print $(NF-3)}') 2>/dev/null; sleep 2; done ) &
blank=$!
trap 'kill $blank 2>/dev/null' EXIT

# a place with a position and a box (the Karlskirche in Vienna), a name and a type
touch Karlskirche
addattr -t mime BEOS:TYPE entity/x-vnd.sen-labs.entity.place Karlskirche
addattr -t string dc:title "Karlskirche" Karlskirche
addattr -t string SEN:placeType "place of worship" Karlskirche
addattr -t double schema:latitude -- 48.1982785 Karlskirche
addattr -t double schema:longitude -- 16.3718951 Karlskirche
addattr -t double SEN:boxSouth -- 48.1977 Karlskirche; addattr -t double SEN:boxNorth -- 48.1988 Karlskirche
addattr -t double SEN:boxWest -- 16.3710 Karlskirche; addattr -t double SEN:boxEast -- 16.3728 Karlskirche

rm -rf "$CACHE"
hey Shoji quit >/dev/null 2>&1; sleep 2
"$SHOJI" "$T/Karlskirche" &
sleep 15
windows=$(hey Shoji COUNT Window 2>/dev/null | sed -n 's/.*: *\([0-9]*\) (0x.*/\1/p' | head -1)
check "the window of the place is open" test "${windows:-0}" -ge 1
check "the tiles of the map are in the cache" test "$(find "$CACHE" -name '*.png' 2>/dev/null | wc -l)" -ge 1
screenshot -s -f png "$T/place-template.png" >/dev/null 2>&1
check "the screenshot was made" test -s "$T/place-template.png"
hey Shoji quit >/dev/null 2>&1

[ $failed = 0 ] && echo "all ok" || echo FAILED
exit $failed
