#ifndef BITCOIN_UTILUTXOCOMMIT_H
#define BITCOIN_UTILUTXOCOMMIT_H

#include <cstddef>

class CCoinsViewCursor;
class CUtxoCommit;

//! Builds a CUtxoCommit from an existing UTXO set.
CUtxoCommit BuildUtxoCommit(CCoinsViewCursor* pcursor, size_t numworkers);

#endif // BITCOIN_UTILUTXOCOMMIT_H
