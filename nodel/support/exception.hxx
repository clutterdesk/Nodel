/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <exception>
#include <string>
#include <sstream>

#define ASSERT(cond) { if (!(cond)) throw ::nodel::Assert{#cond}; }

class NoTraceException : public std::exception
{
  public:
    NoTraceException(std::string&& msg) : m_msg{msg} {}
    const char* what() const noexcept override { return m_msg.data(); }
  private:
    std::string m_msg;
};

#if __has_include("cpptrace/cpptrace.hpp")
#include <cpptrace/cpptrace.hpp>
#define BASE_EXCEPTION cpptrace::exception_with_message
#else
#define BASE_EXCEPTION NoTraceException
#endif

namespace nodel {

class NodelException : public BASE_EXCEPTION
{
  public:
    NodelException(std::string&& msg) : BASE_EXCEPTION(std::forward<std::string>(msg)) {}
    NodelException() : NodelException{""} {}
};


class Assert : public BASE_EXCEPTION
{
  public:
    Assert(std::string&& msg) : BASE_EXCEPTION(std::forward<std::string>(msg)) {}
};


struct WrongType : public NodelException
{
    static std::string make_message(const std::string_view& actual) {
        std::stringstream ss;
        ss << "type=" << actual;
        return ss.str();
    }

    static std::string make_message(const std::string_view& actual, const std::string_view& expected) {
        std::stringstream ss;
        ss << "type=" << actual << ", expected=" << expected;
        return ss.str();
    }

    WrongType(const std::string_view& actual) : NodelException(make_message(actual)) {}
    WrongType(const std::string_view& actual, const std::string_view& expected) : NodelException(make_message(actual, expected)) {}
};

} // nodel namespace
