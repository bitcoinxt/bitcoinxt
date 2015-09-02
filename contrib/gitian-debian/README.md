**Create A Debian Package Installer**

1. Download gitian created bitcoinxt tar file to bitcoinxt/contrib/gitian-debian folder:

  ```
  cd bitcoinxt/contrib/gitian-debian
  wget https://github.com/bitcoinxt/bitcoinxt/releases/download/v0.11A/bitcoin-0.11.0-linux64.tar.gz
  ```

2. Execute debian installer build script:
  ```
  ./build.sh
  ```

**Test New Debian Package Installer**

1. Install newly created debian package on test debian system:

  ```
  sudo gdebi bitcoinxt-0.11A.deb
  ```

2. Verify bitcoinxt daemon installed and started:

  ```
  sudo systemctl status bitcoinxtd
  ```

3. Add your user account to the bitcoin system group:
   
  ```
  sudo usermod -a -G bitcoin <your username>
  ```
  
4. Logout and back into your account so new group assignment takes affect.

5. Verify your username was added to the bitcoin group:

  ```
  groups
  ```

6. Test bitcoinxt-cli access:

  ```
  /usr/bin/bitcoinxt-cli -conf=/etc/bitcoinxt/bitcoin.conf getinfo
  ```
  
7. Test bitcoinxt-qt with non-conflicting IP port:
  
  ```
  bitcoinxt-qt -listen=0:8444
  ```
  
8. Uninstall bitcoinxt without removing config file or data:

  ```
  sudo apt-install uninstall bitcoinxt
  ```

9. Uninstall bitcoinxt AND remove config file and data:

  ```
  sudo apt-install purge bitcoinxt
  sudo rm -rf /var/lib/bitcoinxt
  ```
