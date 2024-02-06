#pragma once

#include <string>
#include <concepts>

namespace nodel {

// std::visit support
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// string quoting, in case std::quoted method is slow
inline
std::string quoted(const std::string& str) {
  std::stringstream ss;
  ss << std::quoted(str);
  return ss.str();
}

} // nodel namespace
