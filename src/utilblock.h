#ifndef UTILBLOCK_H
#define UTILBLOCK_H

#include <vector>
#include <string>
class CTransaction;

// Returns transactions topologically sorted such that any parent transactions
// comes before child transactions spending them
std::vector<CTransaction> SortByParentsFirst(
        std::vector<CTransaction>::const_iterator txBegin,
        std::vector<CTransaction>::const_iterator txEnd);

std::tuple<std::string, std::string, std::string> BlockStatusToStr(uint32_t status);

#endif
