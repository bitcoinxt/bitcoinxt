#ifndef BITCOIN_SIGHASHTYPE_H
#define BITCOIN_SIGHASHTYPE_H

#include <string>
#include <cstdint>

/** Signature hash types/flags */
enum class SigHashType : uint32_t
{
    UNDEFINED = 0,
    ALL = 1,
    NONE = 2,
    SINGLE = 3,
    FORKID = 0x40,
    ANYONECANPAY = 0x80,
};

inline SigHashType operator|(SigHashType lhs, SigHashType rhs) {
    return static_cast<SigHashType>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline SigHashType operator&(SigHashType lhs, SigHashType rhs) {
    return static_cast<SigHashType>(
            static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline SigHashType operator~(SigHashType rhs) {
    return static_cast<SigHashType>(~static_cast<uint32_t>(rhs));
}

inline bool operator!(SigHashType rhs) {
    return !static_cast<uint32_t>(rhs);
}

inline SigHashType& operator|=(SigHashType& lhs, SigHashType rhs) {
    lhs = static_cast<SigHashType>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    return lhs;
}

inline SigHashType& operator&=(SigHashType& lhs, SigHashType rhs) {
    lhs = static_cast<SigHashType>(
        static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    return lhs;
}

SigHashType GetBaseType(SigHashType);
SigHashType RemoveBaseType(SigHashType);

SigHashType FromStr(const std::string& sighashtype);
std::string ToStr(SigHashType);
uint32_t ToInt(SigHashType);

std::ostream& operator<<(std::ostream& stream, const SigHashType& h);

#endif // BITCOIN_SIGHASHTYPE_H
