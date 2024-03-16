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
#include <source_location>

#include <string_view>

namespace nodel {
namespace log {

enum Level
{
    DEBUG,
    WARNING,
    ERROR,
    FATAL,
};

constexpr static int level = DEBUG;

constexpr auto HEADING = "\033[38;5;39m";
constexpr auto MESSAGE = "\033[38;5;39m";
constexpr auto SOURCE = "\033[38;5;7m";
constexpr auto RESTORE = "\033[0m";

inline
std::string_view trim_file_name(const std::source_location& loc) {
    auto fname = loc.file_name();
    auto len = std::strlen(fname);
    return (len > 20)? std::string_view{(fname + len - 20), 20}: std::string_view{fname, len};
}

template <typename ... Args>
void debug(const std::source_location& loc, const char *format, Args&& ... args) {
    if constexpr (level >= Level::DEBUG) {
        std::cout << HEADING << "[DEBUG] " << SOURCE << trim_file_name(loc) << ':' << loc.line() << " - " << MESSAGE <<
            fmt::format(fmt::runtime(format), std::forward<Args>(args)...) << RESTORE << std::endl;
    }
}

template <typename ... Args>
void warn(const std::source_location& loc, const char *format, Args&& ... args) {
    if constexpr (level >= Level::DEBUG) {
        std::cout << HEADING << "[WARNING] " << SOURCE << trim_file_name(loc) << ':' << loc.line() << " - " << MESSAGE <<
            fmt::format(fmt::runtime(format), std::forward<Args>(args)...) << RESTORE << std::endl;
    }
}

template <typename ... Args>
void error(const std::source_location& loc, const char *format, Args&& ... args) {
    if constexpr (level >= Level::DEBUG) {
        std::cout << HEADING << "[ERROR] " << SOURCE << trim_file_name(loc) << ':' << loc.line() << " - " << MESSAGE <<
            fmt::format(fmt::runtime(format), std::forward<Args>(args)...) << RESTORE << std::endl;
    }
}

template <typename ... Args>
void fatal(const std::source_location& loc, const char *format, Args&& ... args) {
    if constexpr (level >= Level::DEBUG) {
        std::cout << HEADING << "[FATAL] " << SOURCE << trim_file_name(loc) << ':' << loc.line() << " - " << MESSAGE <<
            fmt::format(fmt::runtime(format), std::forward<Args>(args)...) << RESTORE << std::endl;
    }
}

#define DEBUG(format, ...) ::nodel::log::debug(std::source_location::current(), format, __VA_ARGS__);
#define WARN(format, ...) ::nodel::log::warn(std::source_location::current(), format, __VA_ARGS__);
#define ERROR(format, ...) ::nodel::log::error(std::source_location::current(), format, __VA_ARGS__);
#define FATAL(format, ...) ::nodel::log::fatal(std::source_location::current(), format, __VA_ARGS__);

} // namespace log
} // namespace nodel

