#include "utilutxocommit.h"
#include "coins.h"
#include "primitives/transaction.h"
#include "utxocommit.h"
#include "util.h"

#include <atomic>
#include <future>
#include <mutex>
#include <stdexcept>

namespace {
static const int WORKER_BUFFER_SIZE = 500;

bool fillBuffer(std::vector<std::pair<COutPoint, Coin>>& buff, CCoinsViewCursor* pcursor) {
    for (size_t n = 0; n < WORKER_BUFFER_SIZE; ++n) {
        std::pair<COutPoint, Coin> utxo;
        if (!pcursor->Valid()) {
            return false;
        }
        if (!pcursor->GetKey(utxo.first) || !pcursor->GetValue(utxo.second)) {
            throw std::runtime_error("failed to retrieve from UTXO cursor");
        }
        buff.emplace_back(std::move(utxo));
        pcursor->Next();
    }
    return true;
}

void logProgress(std::atomic<int>& count, const std::pair<COutPoint, Coin>& utxo) {
    if (count.fetch_add(1, std::memory_order_relaxed) % 1000000 != 0)
        return;
    uint8_t c = *utxo.first.hash.begin();
    LogPrintf("Generating UTXO commitment; progress %d\n",
            uint32_t(c) * 100 / 256);
}

} // ns anon

CUtxoCommit BuildUtxoCommit(CCoinsViewCursor* pcursor, size_t numworkers)
{
    if (numworkers == 0)
        throw std::invalid_argument("numworkers == 0");

    std::atomic<bool> eof(false);
    std::atomic<int> count(0);
    std::mutex cs;

    // spin up workers
    auto worker = [&pcursor, &eof, &count, &cs]() {
        CUtxoCommit commitment;

        while (!eof) {
            std::vector<std::pair<COutPoint, Coin>> utxos;
            utxos.reserve(WORKER_BUFFER_SIZE);
            {
                std::unique_lock<std::mutex> lock(cs);
                if (!fillBuffer(utxos, pcursor)) {
                    eof = true;
                }
            }
            for (auto& utxo : utxos) {
                commitment.Add(utxo.first, utxo.second);
                logProgress(count, utxo);
            }
        }
        return commitment;
    };

    std::vector<std::future<CUtxoCommit> > workers;
    for (size_t i = 0; i < numworkers; ++i) {
        workers.push_back(std::async(std::launch::async, worker));
    }

    // combine results
    CUtxoCommit result;
    for (auto& w : workers) {
        w.wait();
        result.Add(w.get());
    }
    return result;
}
