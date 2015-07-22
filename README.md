*Want to get XT? Download it from https://bitcoinxt.software*

Bitcoin XT
==========

Bitcoin XT is a patch set on top of Bitcoin Core, with a focus on upgrades to the peer to peer protocol. By running it
you can opt in to providing the Bitcoin network with additional services beyond what Bitcoin Core provides.

Currently it contains the following additional changes:

1. *Support for larger blocks.* XT has support for BIP 101 by Gavin Andresen, which schedules an increase from the
   one megabyte limit Bitcoin is now hitting.
2. Relaying of double spends. Bitcoin Core will simply drop unconfirmed transactions that double spend other unconfirmed
   transactions, forcing merchants who want to know about them to connect to thousands of nodes in the hope of spotting
   them. This is unreliable, wastes resources and isn't feasible on mobile devices. Bitcoin XT incorporates work by
   Tom Harding and Gavin Andresen that relays the first observed double spend of a transaction. Additionally, it will
   highlight it in red in the user interface. Other wallets also have the opportunity to use the new information to alert
   the user that there is a fraud attempt against them.
3. Support for querying the UTXO set given an outpoint. This is useful for apps that use partial transactions, such as
   the [Lighthouse](https://github.com/vinumeris/lighthouse) crowdfunding app. The feature allows a client to check that
   a partial SIGHASH_ANYONECANPAY transaction is correctly signed and by querying multiple nodes, build up some
   confidence that the output is not already spent.
4. DNS seed changes: bitseed.xf2.org is removed as it no longer works, and seeds from Addy Yeow and Mike Hearn are
   (re)added to increase seed diversity and redundancy.

XT uses the same data directories as Core so you can easily switch back and forth. You do *not* need to redownload
the block chain.

Bitcoin XT downloads are code signed and are [built reproducibly using gitian](https://github.com/bitcoinxt/gitian.sigs).
If you use it please [sign up to the announcement mailing list](https://bitcoinxt.software) so you can be reminded
of new versions.

The XT Manifesto
================

Bitcoin XT incorporates changes that didn't make it into Core, or are very unlikely to, due to differing philosophies.

The principles XT uses to guide decisions on patches are as follows:

* We try to follow the founding vision of the Bitcoin project, as defined by Satoshi's writings. That means:
  * We support a Bitcoin network in which ordinary people settle with each directly on the block chain.
  * We aim for mainstream adoption, by people who may not be ideologically motivated.
  * We focus on the user experience, as ultimately, how users experience Bitcoin is fundamental to the project's success.
* Better is better. Changes can be useful even if more work remains to be done, or there are theoretically better
  proposals not backed by working code. Put another way, perfect is the enemy of the good.
* Truly mainstream adoption is not inevitable; it requires hard work by everyone. We must never make decisions based on
  the assumption of inevitable success.
* There is a clear decision making process, and product decisions are made in a timely manner.
* We maintain a professional working environment at all times. The developer discussion forum is moderated and
  troublemaking, nastyness, Machiavellianism etc will result in bans.

Development process
===================

To propose a patch for inclusion or to discuss Bitcoin development in general, you are welcome to use the [mailing list](https://groups.google.com/forum/#!forum/bitcoin-xt/).

The repository has a similar structure to upstream, however, because patches are constantly rebased every release
has a branch including the minor ones. XT releases use the upstream version with a letter after them. If two releases
are done based on the same upstream release, the letter is incremented (0.11.0A, 0.11.0B etc).

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

