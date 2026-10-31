#!/bin/sh
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 SEN Labs e.U.
#
# Makes the package of Shoji: the program, its view templates (data/shoji/templates, where Shoji looks for them after the settings of the user) and the
# entry of the Deskbar menu. Needs the built program (make, bin/shoji); the templates are made by the program itself (bin/shoji -g).
# The version comes from the app_version resource: variety 0 is a development version (0.2.0~develop, with the count of `internal` if it is not 0), 2 a beta (0.2.0~beta3), 5 the final one.
# REVISION overrides the revision (default 1).
set -e
cd "$(dirname "$0")"
ROOT=$(pwd)

APP=bin/shoji
RDEF=src/resources/Resources.rdef
[ -f "$APP" ] || { echo "$APP not found, run make first" >&2; exit 1; }

field() { sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*\([0-9]*\).*/\1/p" $RDEF | head -n1; }
VERSION="$(field major).$(field middle).$(field minor)"
internal=$(field internal)
case "$(field variety)" in
	0) suffix=develop; [ "${internal:-0}" -gt 0 ] && suffix=develop.$internal; VERSION="$VERSION~${SEN_VERSION_SUFFIX:-$suffix}" ;;
	2) VERSION="$VERSION~beta${internal:-1}" ;;
esac
VERSION="$VERSION-${REVISION:-1}"
ARCH=$(getarch)

STAGE=$ROOT/package-build
rm -rf $STAGE
mkdir -p $STAGE/apps $STAGE/data/shoji/templates $STAGE/data/deskbar/menu/Applications $STAGE/documentation/packages/shoji

cp -a $APP $STAGE/apps/Shoji
ln -s ../../../../../apps/Shoji $STAGE/data/deskbar/menu/Applications/Shoji

# the templates are archived views: the program makes them (into ./generated of the folder that it is started in)
work=$(mktemp -d)
(cd $work && $ROOT/$APP -g) >/dev/null
[ -d $work/generated ] || { echo "no templates were generated" >&2; exit 1; }
cp -a $work/generated/. $STAGE/data/shoji/templates/
rm -rf $work

cp -a images/shoji-logo.jpg $STAGE/data/shoji/logo.jpg
cp -a README.md LICENSE $STAGE/documentation/packages/shoji/
sed -e "s|@VERSION@|$VERSION|g" -e "s|@ARCH@|$ARCH|g" package/PackageInfo.in > $STAGE/.PackageInfo

PACKAGE=shoji-$VERSION-$ARCH.hpkg
rm -f $PACKAGE
package create -C $STAGE $PACKAGE
rm -rf $STAGE
echo "created $PACKAGE"
