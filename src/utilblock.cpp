#include "utilblock.h"
#include "utilhash.h"
#include "primitives/transaction.h"

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stack>

std::vector<CTransaction> SortByParentsFirst(
        std::vector<CTransaction>::const_iterator txBegin,
        std::vector<CTransaction>::const_iterator txEnd) {
    size_t total_txs = std::distance(txBegin, txEnd);

    std::vector<CTransaction> sorted;
    sorted.reserve(total_txs);

    std::unordered_map<uint256, size_t, SaltedTxIDHasher> index;
    index.reserve(total_txs);
    for (auto i = txBegin; i != txEnd; ++i) {
        index[i->GetHash()] = std::distance(txBegin, i);
    }
    std::unordered_set<size_t> added;
    added.reserve(total_txs);

    std::stack<size_t> queue;

    auto add_or_queue = [&](size_t i) {
        if (added.count(i)) {
            return;
        }
        auto& tx = *(txBegin + i);
        for (auto& in : tx.vin) {
            auto p = index.find(in.prevout.hash);
            if (p == end(index)) {
                continue;
            }
            // tx is child of p, if p isn't added yet, postpone.
            if (added.count(p->second)) {
                continue;
            }

            // go to the queue, but push parent in front of us.
            queue.push(i);
            queue.push(p->second);
            return;
        }
        sorted.push_back(tx);
        added.insert(i);
    };

    for (size_t i = 0; i < total_txs; ++i) {
        queue.push(i);
    }

    while (!queue.empty()) {
        size_t i = queue.top();
        queue.pop();
        add_or_queue(i);
    }
    return sorted;
}
