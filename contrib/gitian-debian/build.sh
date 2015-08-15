#!/bin/bash

#  Shell script that converts gitian built binaries into a simple DPKG, which can then be put into an apt repo.
#
#  Improvement ideas:
#  - Install the man page from the source repo.
#  - Install an init script or systemd file or whatever then hip kids use these days, so it starts at boot.
#  - Wrap in a script that restarts the node if it crashes and/or sends crash reports/core dumps to some issue tracker.
#  - etc ...

ver=0.11.0
realver=0.11A

# Make working space
workdir=bitcoinxt-$realver
[ -d $workdir ] && rm -r $workdir
mkdir $workdir
cd $workdir

# Extract the tarball to a directory called usr
tarball=bitcoin-$ver-linux64.tar.gz
tar xzvf ../$tarball
mv bitcoin-$ver usr

# Rename the binaries so we don't conflict with regular Bitcoin
mv usr/bin/bitcoind usr/bin/bitcoinxt
mv usr/bin/bitcoin-cli usr/bin/bitcoinxt-cli
mv usr/bin/bitcoin-tx usr/bin/bitcoinxt-tx

# Set up debian metadata. There are no dependencies beyond libc and other base DSOs as everything is statically linked.

mkdir DEBIAN
cat <<EOF >DEBIAN/control
Package: bitcoinxt
Architecture: amd64
Description: Bitcoin XT is a fully verifying Bitcoin node implementation, based on the sources of Bitcoin Core.
Maintainer: Mike Hearn <hearn@vinumeris.com>
Version: $realver
EOF

cat <<EOF >DEBIAN/install
usr/bin/bitcoinxt usr/bin
usr/bin/bitcoinxt-cli usr/bin
usr/bin/bitcoinxt-tx usr/bin
EOF

cd ..

# Build deb
dpkg-deb --build $workdir
