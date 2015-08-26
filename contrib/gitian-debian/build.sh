#!/bin/bash

#  Shell script that converts gitian built binaries into a simple DPKG, which can then be put into an apt repo.
#
#  Improvement ideas:
#  - Install the man page from the source repo.
#  - Wrap in a script that sends crash reports/core dumps to some issue tracker.
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

# copy bitcoinxt.service file to lib/systemd/system directory
mkdir -p lib/systemd/system 
cp ../bitcoinxt.service lib/systemd/system

# copy bitcoin.conf file to etc/bitcoinxt 
mkdir -p etc/bitcoinxt
cp ../bitcoin.conf etc/bitcoinxt

# create file to force creation of data folder
mkdir -p var/lib/bitcoinxt
touch var/lib/bitcoinxt/.empty

# Rename the binaries so we don't conflict with regular Bitcoin
mv usr/bin/bitcoind usr/bin/bitcoinxt
mv usr/bin/bitcoin-cli usr/bin/bitcoinxt-cli
mv usr/bin/bitcoin-tx usr/bin/bitcoinxt-tx
mv usr/bin/bitcoin-qt usr/bin/bitcoinxt-qt

# Remove unneeded files 
rm usr/bin/test_bitcoin
rm usr/bin/test_bitcoin-qt
rm usr/include/*
rm usr/lib/*

# Set up debian metadata. There are no dependencies beyond libc and other base DSOs as everything is statically linked.

mkdir DEBIAN
cat <<EOF >DEBIAN/control
Package: bitcoinxt
Architecture: amd64
Description: Bitcoin XT is a fully verifying Bitcoin node implementation, based on the sources of Bitcoin Core.
Maintainer: Steve Myers <steven.myers@gmail.com>
Version: $realver
Depends: adduser, ntp
EOF

cat <<EOF >DEBIAN/install
usr/bin/bitcoinxt usr/bin
usr/bin/bitcoinxt-cli usr/bin
usr/bin/bitcoinxt-tx usr/bin
EOF

cat <<EOF >DEBIAN/conffiles
lib/systemd/system/bitcoinxt.service
etc/bitcoinxt/bitcoin.conf
var/lib/bitcoinxt/.empty
EOF

cat <<EOF >DEBIAN/postinst
#!/bin/bash

# add random rpc password to bitcoin.conf
echo -n "rpcpassword=" >> /etc/bitcoinxt/bitcoin.conf 
bash -c 'tr -dc a-zA-Z0-9 < /dev/urandom | head -c32 && echo' >> /etc/bitcoinxt/bitcoin.conf 

# add users
adduser --system --group --quiet bitcoin

# cleanup permissions
chown root:root /usr/bin/bitcoinxt*
chown root:root /lib/systemd/system/bitcoinxt.service
chown root:root /etc/bitcoinxt
chown bitcoin:bitcoin /etc/bitcoinxt/bitcoin.conf
chmod ug+r /etc/bitcoinxt/bitcoin.conf 
chmod u+w /etc/bitcoinxt/bitcoin.conf
chmod o-rw /etc/bitcoinxt/bitcoin.conf 
chown -R bitcoin:bitcoin /var/lib/bitcoinxt
chmod u+rwx /var/lib/bitcoinxt

# enable and start bitcoinxt service if systemctl exists and is executable
if [[ -x "/bin/systemctl" ]]
then
    /bin/systemctl enable bitcoinxt.service
    /bin/systemctl start bitcoinxt.service
else
    echo "File '/bin/systemctl' is not executable or found, bitcoinxt not automatically enabled and started."
fi

EOF

chmod a+x DEBIAN/postinst 

cat <<EOF >DEBIAN/prerm
#!/bin/bash

# stop and disable bitcoinxt service if systemctl exists and is executable
if [[ -x "/bin/systemctl" ]]
then
    /bin/systemctl stop bitcoinxt.service
    /bin/systemctl disable bitcoinxt.service
else
    echo "File '/bin/systemctl' is not executable or found, bitcoinxt not automatically stopped and disabled."
fi
EOF

chmod a+x DEBIAN/prerm

cd ..

# Build deb
dpkg-deb --build $workdir
