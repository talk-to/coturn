#!/bin/sh
#
# This is an example of a script to run a TURN UDP client that
# authenticates with the long-term credentials mechanism.
#
# Options:
#
# 1) -t is absent, it means that UDP networking is used.
# 2) -e 127.0.0.1 means that the clients will use peer address 127.0.0.1.
# 3) -u ninefingers means that if the server challenges the client
#    with authentication challenge, then we use account "ninefingers".
# 4) -w youhavetoberealistic sets the password for the account as
#    "youhavetoberealistic".
# 5) 127.0.0.1 (the last parameter) is the TURN Server IP address.
#

if [ -d examples ] ; then
       cd examples
fi

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib/

PATH=examples/bin/:../bin/:./bin/:${PATH} turnutils_uclient -e 127.0.0.1 -u ninefingers -w youhavetoberealistic $@ 127.0.0.1
