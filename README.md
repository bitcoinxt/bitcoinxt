Bitcoin XT
==========

Bitcoin XT is an implementation of a Bitcoin Cash (BCH) full node that embraces Bitcoin's original vision of simple, reliable, low-cost transactions for everyone in the world. Bitcoin XT originated as a series of patches on top of Bitcoin Core and is now a independently maintained software fork. See some selected [features](https://bitcoinxt.software/patches.html).

Bitcoin XT downloads are code signed and are [built reproducibly using gitian](https://github.com/bitcoinxt/gitian.sigs). Discussion can be found at the [XT Gitter](https://gitter.im/bitcoinxt/Lobby).

Data Directory Compatibility
============================

If using a data directory from a different BCH or BTC full node, specify `-reindex` on first launch (reason: UTXO database format).

Set aside `wallet.dat` from Bitcoin Core or Bitcoin ABC (reason: HD wallet not yet supported). Also delete `fee_estimates.dat` from these clients.
 
Bitcoin XT can run in BTC chain mode by specifying `-uahftime=0`. Please note that Bitcoin XT does not validate segwit, and the conversion notes above still apply.


Development process
===================

Ideas for useful features are tracked in Issues.  Pull requests from developers, including well-done cherry picks from other clients or cryptos, are very welcome.

To discuss Bitcoin Cash development in general, you are welcome to use the [mailing list](https://lists.linuxfoundation.org/pipermail/bitcoin-ml/).

We have a manifesto that lays out things we believe are important, which you can read about here:

https://bitcoinxt.software/

