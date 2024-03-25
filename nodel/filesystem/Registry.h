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

#include <filesystem>
#include <regex>

#include <nodel/support/Ref.h>
#include <nodel/core/Object.h>

namespace nodel {
namespace filesystem {


class Registry
{
  public:
    using Factory = std::function<DataSource*(const Object&, const std::filesystem::path&)>;

    template <typename Func>
    void add_directory(const std::string_view& extension, Func&& factory) {
        m_dir_reg.emplace(extension, std::forward<Func>(factory));
    }

    template <typename Func>
    void add_directory(const std::regex& regex, Func&& factory) {
        m_dir_regex_list.push_back(std::make_tuple(regex, factory));
    }

    template <typename Func>
    void add_file(const std::string_view& extension, Func&& factory) {
        m_file_reg.emplace(extension, std::forward<Func>(factory));
    }

    template <typename Func>
    void add_file(const std::regex& regex, Func&& factory) {
        m_file_regex_list.push_back(std::make_tuple(regex, factory));
    }

    DataSource* new_directory(const Object& target, const std::filesystem::path& path) const {
        if (auto it = m_dir_reg.find(path.extension().string()); it != m_dir_reg.end()) {
            return it->second(target, path);
        } else {
            auto path_str = path.string();
            for (auto& item : m_dir_regex_list) {
                if (std::regex_search(path_str, std::get<0>(item))) {
                    return std::get<1>(item)(target, path);
                }
            }
        }

        return nullptr;
    }

    DataSource* new_file(const Object& target, const std::filesystem::path& path) const {
        auto ext = path.extension().string();
        if (auto it = m_file_reg.find(ext); it != m_file_reg.end()) {
            return it->second(target, path);
        } else {
            auto path_str = path.string();
            for (auto& item : m_file_regex_list) {
                if (std::regex_search(path_str, std::get<0>(item))) {
                    return std::get<1>(item)(target, path);
                }
            }
        }
        return nullptr;
    }

  protected:
    Registry() = default;

  protected:
    std::unordered_map<std::string, Factory> m_dir_reg;
    std::unordered_map<std::string, Factory> m_file_reg;
    std::vector<std::tuple<std::regex, Factory>> m_dir_regex_list;
    std::vector<std::tuple<std::regex, Factory>> m_file_regex_list;

  private:
    refcnt_t m_ref_count;

  template <typename> friend class ::nodel::Ref;
};

} // namespace filesystem
} // namespace nodel
