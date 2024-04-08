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

#include <rocksdb/db.h>
#include <unordered_map>
#include <filesystem>
#include <nodel/support/types.h>

namespace db = rocksdb;
using namespace nodel;

namespace nodel::rocksdb {

class DBManager
{
  public:
    static DBManager& get_instance();

    class DB
    {
      public:
        DB(db::Options options, db::Status status, db::DB* ptr) : m_options{options}, m_status{status}, m_ptr{ptr}, m_ref_count{1} {}
        ~DB() { delete m_ptr; }

      public:
        db::Options m_options;
        db::Status m_status;
        db::DB* m_ptr;
        refcnt_t m_ref_count;

      template <typename> friend class ::nodel::Ref;
    };

  public:
    DBManager() = default;

    db::Status open(db::Options, const std::filesystem::path&, db::DB*&);
    void close(const std::filesystem::path& path);

  private:
    std::unordered_map<std::string, DB*> m_instances;
};


inline
db::Status DBManager::open(db::Options options, const std::filesystem::path& path, db::DB*& p_db) {
    if (!m_instances.contains(path.string())) {
        db::Status status = db::DB::Open(options, path.string(), &p_db);
        ASSERT(status.ok()); // TODO: error handling
        m_instances.insert(std::make_pair(path.string(), new DB(options, status, p_db)));
        return status;
    } else {
        auto& item = m_instances.at(path.string());
        ++(item->m_ref_count);
        // TODO: check relevant options are identical
        p_db = item->m_ptr;
        return item->m_status;
    }
}

inline
void DBManager::close(const std::filesystem::path& path) {
    auto db_entry = m_instances.at(path.string());
    if (--(db_entry->m_ref_count) == 0) {
        delete db_entry;
        m_instances.erase(path.string());
    }
}

inline
DBManager& DBManager::get_instance() {
    static DBManager instance;
    return instance;
}

} // nodel::rocksdb namespace
