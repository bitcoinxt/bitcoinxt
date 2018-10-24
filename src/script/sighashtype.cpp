#include "script/sighashtype.h"
#include <map>
#include <sstream>
#include <stdexcept>

SigHashType GetBaseType(SigHashType h) {
    return h & static_cast<SigHashType>(0x1f);
}
SigHashType RemoveBaseType(SigHashType h) {
    return h & ~GetBaseType(h);
}

static const std::map<std::string, SigHashType>& GetSigHashTypeMap() {
    using T = SigHashType;
    static std::map<std::string, SigHashType> mapSigHashValues = {
        { "ALL", T::ALL },
        { "ALL|ANYONECANPAY", T::ALL | T::ANYONECANPAY },
        { "ALL|FORKID", T::ALL | T::FORKID },
        { "ALL|FORKID|ANYONECANPAY", T::ALL | T::FORKID | T::ANYONECANPAY },
        { "NONE", T::NONE },
        { "NONE|ANYONECANPAY", T::NONE | T::ANYONECANPAY },
        { "NONE|FORKID", T::NONE | T::FORKID },
        { "NONE|FORKID|ANYONECANPAY", T::NONE | T::FORKID | T::ANYONECANPAY },
        { "SINGLE", T::SINGLE },
        { "SINGLE|ANYONECANPAY", T::SINGLE | T::ANYONECANPAY },
        { "SINGLE|FORKID", T::SINGLE | T::FORKID },
        { "SINGLE|FORKID|ANYONECANPAY", T::SINGLE | T::FORKID | T::ANYONECANPAY } };
    return mapSigHashValues;
}

SigHashType FromStr(const std::string& sighashtype) {
    auto map = GetSigHashTypeMap();
    auto t = map.find(sighashtype);
    if (t == end(map)) {
        throw std::invalid_argument("Invalid sighash type '" + sighashtype + "'");
    }
    return t->second;
}

std::string ToStr(SigHashType h) {
    auto map = GetSigHashTypeMap();
    for (auto& t : map) {
        if (t.second == h) {
            return t.first;
        }
    }
    std::stringstream ss;
    ss << "Invalid sighashtype '" << static_cast<std::uint32_t>(h) << "'";
    throw std::invalid_argument(ss.str());
}

std::uint32_t ToInt(SigHashType h) {
    return static_cast<std::uint32_t>(h);
}

std::ostream& operator<<(std::ostream& stream,
                     const SigHashType& h) {
    try {
        stream << ToStr(h);
    }
    catch (const std::invalid_argument&) {
        stream << ToInt(h);
    }
    return stream;
 }
