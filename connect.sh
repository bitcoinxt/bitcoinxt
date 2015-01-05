#!/bin/sh

#
# Example method to ensure you get connected to other Bitcoin XT nodes and thus  start receiving relayed double spends.
# This is a quick cludge for the early days, in future we could:
#
# - Extend the getaddr protocol to support querying by service bits and then have XT keep an eye out for others via 
#   addr broadcasts. This is the original way it is/was meant to work, but there are lots of addresses and getaddr
#   doesn't let you restrict your query, so it can be hard to find nodes this way when there aren't many of them.
#
# - Add support for Cartographer queries to XT so it can do the equivalent of the command below directly.
#
# Eventually the DNS and HTTP seeding infrastructure could go away, if nodes themselves crawled the network well enough
# that we didn't need separate programs to do it.
#
# The command below doesn't authenticate the answer.

CSV=$( curl -s 'http://main.seed.vinumeris.com:8081/peers.txt?srvmask=3&getutxo=1' )
ADDRS=(${CSV//,/ })
CMDLINE=$( for a in ${ADDRS[@]}; do echo -n "-addnode=$a "; done )
src/bitcoind $CMDLINE $@
