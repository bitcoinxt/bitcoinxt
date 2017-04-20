#ifndef BITCOIN_BIP135UNKNOWNSALERTER_H
#define BITCOIN_BIP135UNKNOWNSALERTER_H

class CBlockIndex;
namespace Consensus { struct Params; }

#include <functional>

/**
 * Samples the last BIT_WARNING_WINDOW blocks for possible unknown feature
 * signalling and triggers warnings.
 */
class BIP135UnknownsAlerter {
public:
    BIP135UnknownsAlerter(const std::function<void(bool)>& alert) :
        alertFunc(alert)
    { }

    void WarnIfUnexpectedVersion(
            const Consensus::Params& params, const CBlockIndex* chainTip);
private:
    std::function<void(bool)> alertFunc;
    bool alertTriggered;
};

#endif
