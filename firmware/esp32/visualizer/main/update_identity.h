#pragma once
#include <string>
namespace update {
// Use the full app descriptor, independent of the truncated panic-log hash setting.
inline std::string identity_hex(const unsigned char* digest) {
    constexpr char hex[]="0123456789abcdef";
    std::string result(64,'0');
    for(unsigned i=0;i<32;i++) {
        result[2*i]=hex[digest[i]>>4];
        result[2*i+1]=hex[digest[i]&15];
    }
    return result;
}
}
