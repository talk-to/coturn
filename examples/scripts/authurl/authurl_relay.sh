#!/bin/sh
#
# This is an example of starting a TURN Server with AUTH URL
# capabilities.
#
# Options:
#
# 1) Listen on IPv4 address 127.0.0.1 ("--listening-ip").
# 2) Use 127.0.0.1 as the IPv4 relay address ("--relay-ip").
# 3) Be moderately verbose ("--verbose").
# 4) Use the long term credentials mechanism ("--lt-cred-mech").
# 5) Use "example.com" as the realm ("--realm").
# 6) Use "127.0.0.1:9876" as the URL for authentication ("--auth-url").

AUTHURL="http://127.0.0.1:9876"

if [ -d examples ] ; then
       cd examples
fi

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib/

##
# On Mac OS X 10.10), this is resulting in the ./bin/turnserver
# failing to start with the following error:
#
#     dyld: Symbol not found: __cg_jpeg_resync_to_restart
#     Referenced from: /System/Library/Frameworks/ImageIO.framework/Versions/A/ImageIO
#     Expected in: /usr/local/lib//libJPEG.dylib
#     in /System/Library/Frameworks/ImageIO.framework/Versions/A/ImageIO
#
#export DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH}:/usr/local/lib/

PATH="./bin/:../bin/:../../bin/:${PATH}" turnserver --listening-ip 127.0.0.1 --relay-ip 127.0.0.1 --verbose --lt-cred-mech --realm 'example.com' --auth-url "${AUTHURL}" $@
