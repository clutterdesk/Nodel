// Copyright 2024 Robert A. Dunnagan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <exception>

#define ASSERT(cond) { if (!(cond)) throw Assert{#cond}; }

class NoTraceException : public std::exception
{
  public:
    NoTraceException(std::string&& msg) : m_msg{msg} {}
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
