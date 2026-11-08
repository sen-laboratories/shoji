#!/bin/sh
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 SEN Labs e.U.
#
# The styles of the map of a place in Shoji: the style that is chosen in the view (kept in the settings of Shoji) wins, else the default that is set in the settings of the spatial navigator
# (`map_style`), else OpenTopoMap. For each a window of a place is opened, and the tiles of that style (and no other) reach the cache, in the folder of the style.
# The screenshots map-<style>.png are left in TEST_DIR to look at. Run it in the Haiku VM with Shoji installed (or SHOJI=<path>), with a network connection (a few tiles of each public server).
# The settings of Shoji and of the navigator are put back at the end. Prints `ok` or `FAIL` for each check, `FAILED` at the end.
HERE=$(cd "$(dirname "$0")" && pwd)
SHOJI=${SHOJI:-/boot/system/apps/Shoji}
T=${TEST_DIR:-/Develop/test}/_shoji-map-styles
CACHE=$HOME/config/cache/shoji/tiles
SETTINGS=$HOME/config/settings/shoji/settings
PLUGIN=$HOME/config/settings/sen/spatial-navigator.settings
rm -rf "$T"; mkdir -p "$T"; cd "$T" || exit 1
failed=0
check() { desc=$1; shift; if "$@" >/dev/null 2>&1; then echo "ok   $desc"; else echo "FAIL $desc"; failed=1; fi; }

g++ -std=c++17 -o mapstyle-set "$HERE/mapstyle-set.cpp" -lbe 2>&1 | grep error
[ -x ./mapstyle-set ] || { echo "FAIL mapstyle-set was not built"; exit 1; }

( while true; do kill -9 $(ps | grep "[s]creen_blanker" | awk '{print $(NF-3)}') 2>/dev/null; sleep 2; done ) &
blank=$!
[ -f "$SETTINGS" ] && cp -a "$SETTINGS" "$T/settings.saved"
[ -f "$PLUGIN" ] && cp -a "$PLUGIN" "$T/plugin.saved"
restore() {
	kill $blank 2>/dev/null
	if [ -f "$T/settings.saved" ]; then cp -a "$T/settings.saved" "$SETTINGS"; else rm -f "$SETTINGS"; fi
	if [ -f "$T/plugin.saved" ]; then cp -a "$T/plugin.saved" "$PLUGIN"; else rm -f "$PLUGIN"; fi
}
mkdir -p "$(dirname "$PLUGIN")"
grep -v '^map_style' "$PLUGIN" > "$T/plugin.plain" 2>/dev/null || : > "$T/plugin.plain"
cp "$T/plugin.plain" "$PLUGIN"
trap restore EXIT

# the Karlskirche in Vienna: a position and a box
touch Karlskirche
addattr -t mime BEOS:TYPE entity/x-vnd.sen-labs.entity.place Karlskirche
addattr -t string dc:title "Karlskirche" Karlskirche
addattr -t string SEN:placeType "place of worship" Karlskirche
addattr -t double schema:latitude -- 48.1982785 Karlskirche
addattr -t double schema:longitude -- 16.3718951 Karlskirche
addattr -t double SEN:boxSouth -- 48.1977 Karlskirche; addattr -t double SEN:boxNorth -- 48.1988 Karlskirche
addattr -t double SEN:boxWest -- 16.3710 Karlskirche; addattr -t double SEN:boxEast -- 16.3728 Karlskirche

for style in topo osm hot cyclosm; do
	hey Shoji quit >/dev/null 2>&1; sleep 2
	rm -rf "$CACHE"
	./mapstyle-set $style
	"$SHOJI" "$T/Karlskirche" &
	sleep 18
	check "$style: its tiles are in the cache" test "$(find "$CACHE/$style" -name '*.png' 2>/dev/null | wc -l)" -ge 1
	others=$(find "$CACHE" -name '*.png' 2>/dev/null | grep -vc "/tiles/$style/")
	check "$style: and no tile of another style" test "$others" -eq 0
	screenshot -s -f png "$T/map-$style.png" >/dev/null 2>&1
	check "$style: the screenshot was made" test -s "$T/map-$style.png"
	hey Shoji quit >/dev/null 2>&1
done

# the default in the settings of the navigator is used while no style was chosen in the view (an empty choice), and the choice in the view wins
show() { # show <tiles of the style that is expected> <description>
	hey Shoji quit >/dev/null 2>&1; sleep 2
	rm -rf "$CACHE"
	"$SHOJI" "$T/Karlskirche" &
	sleep 18
	check "$2" test "$(find "$CACHE/$1" -name '*.png' 2>/dev/null | wc -l)" -ge 1
	hey Shoji quit >/dev/null 2>&1
}
./mapstyle-set ""
cp "$T/plugin.plain" "$PLUGIN"; echo "map_style = hot" >> "$PLUGIN"
show hot "the default of the navigator settings (hot) is used while nothing was chosen in the view"
./mapstyle-set osm
show osm "the style chosen in the view (osm) wins over the default of the navigator settings (hot)"
./mapstyle-set ""
cp "$T/plugin.plain" "$PLUGIN"; echo "map_style = nowhere" >> "$PLUGIN"
show topo "a default that is not known in the navigator settings is OpenTopoMap"
./mapstyle-set nowhere
cp "$T/plugin.plain" "$PLUGIN"
show topo "a style that is not known in the settings of Shoji is the default (topo)"

[ $failed = 0 ] && echo "all ok" || echo FAILED
