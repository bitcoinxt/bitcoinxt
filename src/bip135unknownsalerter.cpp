#include "bip135unknownsalerter.h"
#include "chain.h"
#include "versionbits.h"
#include "main.h" // for ComputeBlockVersion

#include <tuple>

static std::tuple<size_t, int32_t, int32_t> SampleVersionBits(
        const Consensus::Params& params, const CBlockIndex* chainTip)
{
    if (chainTip == nullptr) {
        throw std::invalid_argument("chainTip == nullptr");
    }

    size_t unexpected = 0;

    // initialized to consensus invalid value, may change in future fork
    int32_t expected_sample = 0;
    int32_t unexpected_sample = 0;

    const CBlockIndex* walk = chainTip;
    for (size_t i = 0; i < BIT_WARNING_WINDOW; ++i) {
        walk = chainTip->pprev;
        if (walk == nullptr) {
            break;
        }
        if (walk->nVersion <= VERSIONBITS_LAST_OLD_BLOCK_VERSION) {
            // non-BIP135 block version
            continue;
        }

        int32_t expected_version = ComputeBlockVersion(walk, params);
        if ((walk->nVersion & ~expected_version) == 0) {
            // as expected
            continue;
        }

        ++unexpected;
        if (expected_version != 0) {
            // sample latest mismatch (first instance in this loop)
            expected_sample = expected_version;
            unexpected_sample = walk->nVersion;

        }
    }
    return std::make_tuple( unexpected, expected_sample, unexpected_sample);
}

void BIP135UnknownsAlerter::WarnIfUnexpectedVersion(
        const Consensus::Params& params, const CBlockIndex* chainTip)
{
    if (chainTip == nullptr || chainTip->nHeight < BIT_WARNING_WINDOW)
        return;

    size_t unexpected;
    using version = int32_t;
    version expected_sample;
    version unexpected_sample;

    std::tie(unexpected, expected_sample, unexpected_sample)
        = SampleVersionBits(params, chainTip);

    if (unexpected == 0) {
        return;
    }

    LogPrintf("BIP135: %d out of last %d blocks had unexpected version. "
              "Expected 0x%x, last unexpected version is: 0x%x\n",
              unexpected, BIT_WARNING_WINDOW, expected_sample, unexpected_sample);

    if (unexpected < static_cast<size_t>(BIT_WARNING_WINDOW / 2)) {
        return;
    }

    // Half of the last 100 blocks have unknown version. Call alert func.
    alertFunc(alertTriggered);
    alertTriggered = true;
}
