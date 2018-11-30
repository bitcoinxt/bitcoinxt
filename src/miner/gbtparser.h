#ifndef BITCOIN_GBT_PARSER_H
#define BITCOIN_GBT_PARSER_H

class CBlock;
class UniValue;

namespace miner {

// Given a JSON object compatible with 'getblocktemplate', this will generate
// a block candidate.
//
// Coinbase is anyone-can-spend
CBlock ParseGBT(const UniValue& gbt);

}

#endif
