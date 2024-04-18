#pragma once

#include <exception>

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

} // nodel namespace
