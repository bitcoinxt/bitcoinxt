#include "utilblock.h"
#include "utilhash.h"
#include "primitives/transaction.h"
#include "chain.h"

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

static std::string BlockValidityStr(BlockStatus s) {
    std::vector<std::string> checks;

    uint32_t validstate = s & BlockStatus::BLOCK_VALID_MASK;

    switch (validstate) {
        case 5:
            checks.push_back("Valid scripts and signatures");
            // fallthrough
        case 4:
            checks.push_back("Valid coin spends");
            // fallthrough
        case 3:
            checks.push_back("Valid transactions");
            // fallthrough
        case 2:
            checks.push_back("Parent headers are valid");
            // fallthrough
        case 1:
            checks.push_back("Valid header");
            break;
        default:
            checks.push_back("UNKNOWN");
    };


    std::string res;
    for (auto c = checks.rbegin(); c != checks.rend(); ++c) {
        if (!res.empty()) {
            res += "; ";
        }
        res += *c;
    }
    return res;
}

static std::string BlockFailureStr(BlockStatus s) {
    std::string res;
    if (s & BlockStatus::BLOCK_FAILED_VALID) {
        res = "Block failed a validity check";
    }
    if (s & BlockStatus::BLOCK_FAILED_CHILD) {
        if (!res.empty()) {
            res += "; ";
        }
        res += "Block descends from a failed block";
    }
    return res;
}

static std::string BlockDataAvailStr(BlockStatus s) {
    std::string res;
    if (s & BlockStatus::BLOCK_HAVE_DATA) {
        res = "Block data available";
    }
    if (s & BlockStatus::BLOCK_FAILED_CHILD) {
        if (!res.empty()) {
            res += "; ";
        }
        res += "Undo data available";
    }
    return res;
}

std::tuple<std::string, std::string, std::string> BlockStatusToStr(uint32_t status) {
    BlockStatus s = BlockStatus(status);
    return std::make_tuple(BlockValidityStr(s),
                           BlockFailureStr(s),
                           BlockDataAvailStr(s));
}
