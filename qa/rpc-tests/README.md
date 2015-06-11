Regression tests of RPC interface
=================================

### [python-bitcoinrpc](https://github.com/jgarzik/python-bitcoinrpc)
Git subtree of [https://github.com/jgarzik/python-bitcoinrpc](https://github.com/jgarzik/python-bitcoinrpc).
Changes to python-bitcoinrpc should be made upstream, and then
pulled here using git subtree.

### [test_framework/test_framework.py](test_framework/test_framework.py)
Base class for new regression tests.

### [test_framework/util.py](test_framework/util.py)
Generally useful functions.

Bash-based tests, to be ported to Python:
-----------------------------------------
- conflictedbalance.sh : More testing of malleable transaction handling

Notes
=====

You can run any single test by calling qa/pull-tester/rpc-tests.py <testname>

Or you can run any combination of tests by calling `qa/pull-tester/rpc-tests.py <testname1> <testname2> <testname3> ...`

Run the regression test suite with `qa/pull-tester/rpc-tests.py'

Run all possible tests with `qa/pull-tester/rpc-tests.py -extended`

Possible options:

```
-h, --help       show this help message and exit
  --nocleanup      Leave bitcoinds and test.* datadir on exit or error
  --noshutdown     Don't stop bitcoinds after the test execution
  --srcdir=SRCDIR  Source directory containing bitcoind/bitcoin-cli (default:
                   ../../src)
  --tmpdir=TMPDIR  Root directory for datadirs
  --tracerpc       Print out all RPC calls as they are made
```

If you set the environment variable `PYTHON_DEBUG=1` you will get some debug output (example: `PYTHON_DEBUG=1 qa/pull-tester/rpc-tests.py wallet`). 

A 200-block -regtest blockchain and wallets for four nodes
is created the first time a regression test is run and
is stored in the cache/ directory. Each node has 25 mature
blocks (25*50=1250 BTC) in its wallet.

After the first run, the cache/ blockchain and wallets are
copied into a temporary directory and used as the initial
test state.

* ```mininode.py``` contains all the definitions for objects that pass
over the network (```CBlock```, ```CTransaction```, etc, along with the network-level
wrappers for them, ```msg_block```, ```msg_tx```, etc).

* P2P tests have two threads.  One thread handles all network communication
with the bitcoind(s) being tested (using python's asyncore package); the other
implements the test logic.

* ```NodeConn``` is the class used to connect to a bitcoind.  If you implement
a callback class that derives from ```NodeConnCB``` and pass that to the
```NodeConn``` object, your code will receive the appropriate callbacks when
events of interest arrive.

* You can pass the same handler to multiple ```NodeConn```'s if you like, or pass
different ones to each -- whatever makes the most sense for your test.

* Call ```NetworkThread.start()``` after all ```NodeConn``` objects are created to
start the networking thread.  (Continue with the test logic in your existing
thread.)

* RPC calls are available in p2p tests.

* Can be used to write free-form tests, where specific p2p-protocol behavior
is tested.  Examples: ```p2p-accept-block.py```, ```maxblocksinflight.py```.

## Comptool

* Testing framework for writing tests that compare the block/tx acceptance
behavior of a bitcoind against 1 or more other bitcoind instances, or against
known outcomes, or both.

* Set the ```num_nodes``` variable (defined in ```ComparisonTestFramework```) to start up
1 or more nodes.  If using 1 node, then ```--testbinary``` can be used as a command line
option to change the bitcoind binary used by the test.  If using 2 or more nodes,
then ```--refbinary``` can be optionally used to change the bitcoind that will be used
on nodes 2 and up.

* Implement a (generator) function called ```get_tests()``` which yields ```TestInstance```s.
Each ```TestInstance``` consists of:
  - a list of ```[object, outcome, hash]``` entries
    * ```object``` is a ```CBlock```, ```CTransaction```, or
    ```CBlockHeader```.  ```CBlock```'s and ```CTransaction```'s are tested for
    acceptance.  ```CBlockHeader```s can be used so that the test runner can deliver
    complete headers-chains when requested from the bitcoind, to allow writing
    tests where blocks can be delivered out of order but still processed by
    headers-first bitcoind's.
    * ```outcome``` is ```True```, ```False```, or ```None```.  If ```True```
    or ```False```, the tip is compared with the expected tip -- either the
    block passed in, or the hash specified as the optional 3rd entry.  If
    ```None``` is specified, then the test will compare all the bitcoind's
    being tested to see if they all agree on what the best tip is.
    * ```hash``` is the block hash of the tip to compare against. Optional to
    specify; if left out then the hash of the block passed in will be used as
    the expected tip.  This allows for specifying an expected tip while testing
    the handling of either invalid blocks or blocks delivered out of order,
    which complete a longer chain.
  - ```sync_every_block```: ```True/False```.  If ```False```, then all blocks
    are inv'ed together, and the test runner waits until the node receives the
    last one, and tests only the last block for tip acceptance using the
    outcome and specified tip.  If ```True```, then each block is tested in
    sequence and synced (this is slower when processing many blocks).
  - ```sync_every_transaction```: ```True/False```.  Analogous to
    ```sync_every_block```, except if the outcome on the last tx is "None",
    then the contents of the entire mempool are compared across all bitcoind
    connections.  If ```True``` or ```False```, then only the last tx's
    acceptance is tested against the given outcome.

* For an example of a test written in this framework, see
  ```invalidblockrequest.py```.

If you get into a bad state, you should be able
to recover with:

```bash
rm -rf cache
killall bitcoind
```
