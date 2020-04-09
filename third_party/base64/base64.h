//
//  base64 encoding and decoding with C++.
//  Version: 1.01.00
//

#pragma once

#include <string>

namespace base64 {

std::string Encode(unsigned char const* , unsigned int len);
std::string Decode(std::string const& s);

inline std::string Encode(const std::string& str)
{ 
    return Encode((const unsigned char*)str.data(), str.size());
}

} // namespace


