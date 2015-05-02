#!/bin/bash

# clean up
#if [ -d ./debian ]; then
#	rm -rf debian
#fi

if [ -d ./deb_dist ]; then
        rm -rf deb_dist
fi

if [ -d ./dist ]; then
        rm -rf dist
fi

#rm guh-cli-*

# build package
#python setup.py --command-packages=stdeb.command debianize

#python setup.py --command-packages=stdeb.command sdist_dsc

python setup.py --command-packages=stdeb.command bdist_deb

