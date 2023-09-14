#pragma once

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <concepts>
#include <fmt/core.h>
#include <memory>
#include <variant>
#include <optional>
#include <functional>

#include "support.h"
#include "Object.h"


 template <typename T>
 concept arithmetic = std::is_arithmetic<T>::value;

 template <typename T>
 requires arithmetic<T>
 class Value
 {
   public:
     Value() noexcept : v(0) {}
     Value(T v) noexcept : v(v) {}
     Value(Reference parent) noexcept : parent{parent}, v(0) {}
     Value(Reference parent, T v) noexcept : parent(parent), v(v) {}
     Value(const Value& other) noexcept : v{other.v} {};

     operator T () const { return v; }

     auto operator ! () const { return !v; }
     auto operator ~ () const { return ~v; }
     auto operator - () const { return -v; }

     Value& operator ++ () { return ++v, *this; }
     Value operator ++ (int) { return *this, ++v; }
     Value& operator -- () { return --v, *this; }
     Value operator -- (int) { return *this, --v; }

     template <typename U, typename V> friend auto operator + (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator - (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator * (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator / (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator % (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator << (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator >> (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator & (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator | (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator ^ (Value<U> lhs, V rhs);
     template <typename U, typename V> friend auto operator <=> (Value<U> lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator += (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator -= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator *= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator /= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator %= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator <<= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator >>= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator &= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator |= (Value<U>& lhs, V rhs);
     template <typename U, typename V> friend Value<U>& operator ^= (Value<U>& lhs, V rhs);

   private:
     Reference parent;
     T v;
 };

 template <typename U, typename V> auto operator + (Value<U> lhs, V rhs) { return lhs.v + rhs; }
 template <typename U, typename V> auto operator - (Value<U> lhs, V rhs) { return lhs.v - rhs; }
 template <typename U, typename V> auto operator * (Value<U> lhs, V rhs) { return lhs.v * rhs; }
 template <typename U, typename V> auto operator / (Value<U> lhs, V rhs) { return lhs.v / rhs; }
 template <typename U, typename V> auto operator % (Value<U> lhs, V rhs) { return lhs.v % rhs; }
 template <typename U, typename V> auto operator << (Value<U> lhs, V rhs) { return lhs.v << rhs; }
 template <typename U, typename V> auto operator >> (Value<U> lhs, V rhs) { return lhs.v >> rhs; }
 template <typename U, typename V> auto operator & (Value<U> lhs, V rhs) { return lhs.v & rhs; }
 template <typename U, typename V> auto operator | (Value<U> lhs, V rhs) { return lhs.v | rhs; }
 template <typename U, typename V> auto operator ^ (Value<U> lhs, V rhs) { return lhs.v ^ rhs; }
 template <typename U, typename V> auto operator <=> (Value<U> lhs, V rhs) { return lhs.v <=> rhs; }
 template <typename U, typename V> Value<U>& operator += (Value<U>& lhs, V rhs) { lhs.v += rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator -= (Value<U>& lhs, V rhs) { lhs.v -= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator *= (Value<U>& lhs, V rhs) { lhs.v *= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator /= (Value<U>& lhs, V rhs) { lhs.v /= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator %= (Value<U>& lhs, V rhs) { lhs.v %= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator <<= (Value<U>& lhs, V rhs) { lhs.v <<= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator >>= (Value<U>& lhs, V rhs) { lhs.v >>= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator &= (Value<U>& lhs, V rhs) { lhs.v &= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator |= (Value<U>& lhs, V rhs) { lhs.v |= rhs; return lhs; }
 template <typename U, typename V> Value<U>& operator ^= (Value<U>& lhs, V rhs) { lhs.v ^= rhs; return lhs; }

 void foo(int x)
 {
     fmt::print("int {}\n", x);
 }

 void foo(double x)
 {
     fmt::print("double: {}\n", x);
 }

 void gen_bin_decl(std::string op) {
     std::cout << "    template <typename U, typename V> friend auto operator " << op << " (Value<U> lhs, V rhs);" << std::endl;
 }

 void gen_bin_defn(std::string op) {
     std::cout << "template <typename U, typename V> auto operator " << op << " (Value<U> lhs, V rhs) { return lhs.v " << op << " rhs; }" << std::endl;
 }

 void gen_bineq_decl(std::string op) {
     std::cout << "    template <typename U, typename V> friend Value<U>& operator " << op << " (Value<U>& lhs, V rhs);" << std::endl;
 }

 void gen_bineq_defn(std::string op) {
     std::cout << "template <typename U, typename V> Value<U>& operator " << op << " (Value<U>& lhs, V rhs) { lhs.v " << op << " rhs; return lhs; }" << std::endl;
 }

 void gen() {
     std::vector<std::string> bin_ops = {"+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^", "<=>"};
     std::vector<std::string> bineq_ops = {"+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^="};

     std::cout << std::endl;
     for (auto& op : bin_ops) gen_bin_decl(op);
     for (auto& op : bineq_ops) gen_bineq_decl(op);
     std::cout << std::endl;

     for (auto& op : bin_ops) gen_bin_defn(op);
     for (auto& op : bineq_ops) gen_bineq_defn(op);
     std::cout << std::endl;
 }


