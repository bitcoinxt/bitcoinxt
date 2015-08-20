*Want to get XT? Download it from https://bitcoinxt.software*

Bitcoin XT
==========

Bitcoin XT is a patch set on top of Bitcoin Core. It implements various changes which you can read about on the website:

https://bitcoinxt.software/patches.html

Development process
===================

To propose a patch for inclusion or to discuss Bitcoin development in general, you are welcome to use the [mailing list](https://groups.google.com/forum/#!forum/bitcoin-xt/).

The repository has a similar structure to upstream, however, because patches are constantly rebased every release
has a branch including the minor ones. XT releases use the upstream version with a letter after them. If two releases
are done based on the same upstream release, the letter is incremented (0.11.0A, 0.11.0B etc).

We have a manifesto that lays out things we believe are important, which you can read about here:

https://bitcoinxt.software/

About double spend relaying
===========================

You can view [a dashboard analysing observed double spends](http://respends.thinlink.com/) that is based on this work. Double spends are relayed by nodes probabilistically to minimise DoS risk: you cannot force the network to use up all its available bandwidth by double spending the same output over and over again due to a rate limiting Bloom filter.

Currently, support in wallets for showing double spends in the UI is inconsistent. We hope that double spend relaying will make it easier for developers to deploy this feature. 

Note that double spend relaying does not prevent double spending, as in the case of a network race you cannot know which transaction will make it into the next block. Additionally double spending by being a malicious miner or exploiting one that isn't following the normal network rules is still possible without triggering an alert. However the feature is still useful for quickly detecting certain types of attack and once you know about the attempted double spend, as a seller you can refuse to process the transaction. Other types of double spending attack are either detectable or harder to pull off, so by increasing information available to wallets the bar for engaging in payment fraud is raised.

Double spend relaying was developed by Gavin Andresen and Tom Harding.

About UTXO set queries
======================

Bitcoin XT supports [BIP 64](https://github.com/bitcoin/bips/blob/master/bip-0064.mediawiki), and so it supports looking up entries in the UTXO database given an outpoint (a transaction id and output index pair). This can be useful for contracts based applications. An example follows.

The Bitcoin protocol allows people to collaborate on building transactions in various ways. One of those ways (the SIGHASH_ANYONECANPAY flag) allows people to create partially signed invalid transactions that cannot be redeemed unless they are combined with other similar transactions, allowing for the creation of a decentralised *assurance contracts*: usually better known as Kickstarter style all-or-nothing crowdfunds. [The protocol](https://en.bitcoin.it/wiki/Contracts#Example_3:_Assurance_contracts) has been implemented in a specialised GUI wallet application called [Lighthouse](https://www.vinumeris.com/lighthouse), which makes creating projects and pledging to them with partial Bitcoin transactions easy.

Because a pledge is an invalid Bitcoin transaction, it cannot be redeemed immediately and the money remains under the control of the pledger. By double spending the pledged outputs,they can *revoke* their pledge and take back the money. This is useful if it looks like the project isn't going to meet its goal any time soon, or if the pledger suddenly needs the money for something else. Thus people collecting pledges need to know whether a pledge can actually be redeemed or not. If a pledge is double spent after being submitted then this can be detected by watching the block chain, but if the pledge was revoked before the project became aware of it, or was simply never valid to begin with, then that doesn't work. Instead the receiver of the pledge needs to check the ledger directly.

The getutxo p2p protocol message allows clients to request the look up the contents of ledger entries. The data that is found there can then be combined with the submitted pledge to check that the pledge is correctly signed (i.e. not attempting to spend someone elses money).

Possible future features
========================

Ideas for useful protocol upgrades are tracked in the issue tracker.

Bitcoin XT is intended to be compatible with crowd funded development. If you would like to experiment with a (non consensus changing) protocol upgrade, please discuss it on the mailing list first. You should be able to get a clear decision on the concept and design before starting on the implementation.

