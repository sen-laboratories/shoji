#!/bin/bash
#
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2024-2026 SEN Labs e.U.

# a simple wrapper for make, since I found no way to add this to the Makefile template
# generates templates and copies them to the user settings directory.
# (the headers of sento are installed outside the tree: make does not see them change, and a class that is all in a header must have one layout in every object that uses it)
find src -name '*.cpp' -exec touch {} +
make && \
make bindcatalogs && \
bin/shoji -g && \
mkdir -p $HOME/config/settings/shoji/templates
cp -a generated/* ~/config/settings/shoji/templates/
