#!/bin/bash

# a simple wrapper for make, since I found no way to add this to the Makefile template
# generates templates and copies them to the user settings directory.
make && \
bin/shoji -g && \
mkdir -p $HOME/config/settings/shoji/templates
cp -a generated/* ~/config/settings/shoji/templates/
