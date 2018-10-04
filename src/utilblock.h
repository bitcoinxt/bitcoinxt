#ifndef UTILBLOCK_H
#define UTILBLOCK_H

#include <vector>
class CTransaction;

// Returns transactions topologically sorted such that any parent transactions
// comes before child transactions spending them
std::vector<CTransaction> SortByParentsFirst(
        std::vector<CTransaction>::const_iterator txBegin,
        std::vector<CTransaction>::const_iterator txEnd);

#endif
