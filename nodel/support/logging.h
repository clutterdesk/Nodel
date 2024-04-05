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

#include <fmt/format.h>
#include <string_view>

namespace nodel {
namespace log {

enum Level
{
    FATAL,
    ERROR,
    WARNING,
    DEBUG,
};

constexpr static int level = DEBUG;

constexpr auto HEADING = "\033[38;5;39m";
constexpr auto MESSAGE = " \033[38;5;39m";
constexpr auto SOURCE = "\033[38;5;7m";
constexpr auto RESTORE = "\033[0m";

inline
std::string_view trim_file_name(const char* file) {
    auto len = std::strlen(file);
    return (len > 20)? std::string_view{(file + len - 20), 20}: std::string_view{file, len};
}

template <typename Arg>
void log(const char* file, int line, const char* level_name, Arg&& arg) {
    std::cout << HEADING << level_name << SOURCE << trim_file_name(file) << ':' << line << MESSAGE <<
        std::forward<Arg>(arg) << RESTORE << std::endl;
}

template <typename ... Args>
void log(const char* file, int line, const char* level_name, const char *format, Args&& ... args) {
    std::cout << HEADING << level_name << SOURCE << trim_file_name(file) << ':' << line << MESSAGE <<
        fmt::format(fmt::runtime(format), std::forward<Args>(args)...) << RESTORE << std::endl;
}

#define DEBUG(...) { if (::nodel::log::level >= ::nodel::log::Level::DEBUG)   ::nodel::log::log(__FILE__, __LINE__, "[DEBUG] ",   __VA_ARGS__); }
#define WARN(...)  { if (::nodel::log::level >= ::nodel::log::Level::WARNING) ::nodel::log::log(__FILE__, __LINE__, "[WARNING] ", __VA_ARGS__); }
#define ERROR(...) { if (::nodel::log::level >= ::nodel::log::Level::ERROR)   ::nodel::log::log(__FILE__, __LINE__, "[ERROR] ",   __VA_ARGS__); }
#define FATAL(...) { if (::nodel::log::level >= ::nodel::log::Level::FATAL)   ::nodel::log::log(__FILE__, __LINE__, "[FATAL] ",   __VA_ARGS__); }

} // namespace log
} // namespace nodel

