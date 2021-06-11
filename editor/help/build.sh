#!/bin/bash

echo "hi"

# https://doc.qt.io/qt-5/qthelp-framework.html

# build a compiled help file based on the help project description file (.qhp)
qhelpgenerator help.qhp -o help.qch

# combine compiled help files into a help collection.
qhelpgenerator help.qhcp -o help.qhc

cp help.qhc ../dist/help
