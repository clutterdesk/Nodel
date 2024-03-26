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
#include <nodel/core/Object.h>
#include <nodel/filesystem/Directory.h>
#include <nodel/serialization/json.h>
#include "DBManager.h"

#include <filesystem>

namespace nodel::rocksdb {

// A simple key/value store backed by RocksDB.
// - Multiple objects may operate on the same DB, but no synchronization is provided.
//   Use Object::refresh or Object::reset to synchronize one object after changes have
//   been made to the DB.
// - Updates and deletes are batched together when Object::save is called, providing
//   atomicity.
class KeyStore : public nodel::DataSource
{
  public:
    KeyStore(const std::filesystem::path& path, Origin origin);
    KeyStore(const std::filesystem::path& path) : KeyStore{path, Origin::MEMORY} {}
    KeyStore(Origin origin)                     : KeyStore{{}, origin} {}
    KeyStore()                                  : KeyStore{{}, Origin::MEMORY} {}

    ~KeyStore();

    void set_db_options(::rocksdb::Options options)         { m_options = options; }
    void set_read_options(::rocksdb::ReadOptions options)   { m_read_options = options; }
    void set_write_options(::rocksdb::WriteOptions options) { m_write_options = options; }

    DataSource* new_instance(const Object& target, Origin origin) const override {
        return new KeyStore(std::filesystem::path{}, origin);
    }

    void read_type(const Object& target) override { ASSERT(false); }
    void read(const Object& target) override {}  // TODO: implement
    void write(const Object&, const Object& cache, bool quiet) override {} // TODO: implement

    Object read_key(const Object&, const Key& k) override;
    void write_key(const Object&, const Key& k, const Object& v, bool quiet) override;
    void commit(const Object& target, const KeyList& del_keys, bool quiet) override;

    class KeyIterator : public nodel::DataSource::KeyIterator
    {
      public:
        KeyIterator(::rocksdb::Iterator* p_iter);
        bool next_impl() override;
      private:
        ::rocksdb::Iterator* mp_iter;
    };

    class ValueIterator : public nodel::DataSource::ValueIterator
    {
      public:
        ValueIterator(::rocksdb::Iterator* p_iter);
        bool next_impl() override;
      private:
        ::rocksdb::Iterator* mp_iter;
    };

    class ItemIterator : public nodel::DataSource::ItemIterator
    {
      public:
        ItemIterator(::rocksdb::Iterator* p_iter);
        bool next_impl() override;
      private:
        ::rocksdb::Iterator* mp_iter;
    };

    std::unique_ptr<nodel::DataSource::KeyIterator> key_iter() override     { return std::make_unique<KeyIterator>(mp_db->NewIterator(m_read_options)); }
    std::unique_ptr<nodel::DataSource::ValueIterator> value_iter() override { return std::make_unique<ValueIterator>(mp_db->NewIterator(m_read_options)); }
    std::unique_ptr<nodel::DataSource::ItemIterator> item_iter() override   { return std::make_unique<ItemIterator>(mp_db->NewIterator(m_read_options)); }

  private:
    void open(const std::filesystem::path& path, bool create_if_missing);

    static std::string serialize_key(const Key&);
    static std::string serialize_value(const Object&);
    static Key deserialize_key(const std::string_view&);
    static Object deserialize_value(const std::string_view&);

  private:
    std::filesystem::path m_path;
    std::filesystem::path m_open_path;
    ::rocksdb::DB* mp_db = nullptr;
    ::rocksdb::Options m_options;
    ::rocksdb::ReadOptions m_read_options;
    ::rocksdb::WriteOptions m_write_options;
    ItemList updates;
};


inline
KeyStore::KeyStore(const std::filesystem::path& path, Origin origin)
  : nodel::DataSource(Kind::SPARSE, Mode::READ | Mode::WRITE, Object::OMAP_I, origin)
  , m_path{path} {
    m_options.error_if_exists = false;
    if (!path.empty()) {
        open(path, true);
    }
}

inline
KeyStore::~KeyStore() {
    if (mp_db != nullptr) {
        DBManager::get_instance().close(m_open_path);
        mp_db = nullptr;
    }
}

inline
KeyStore::KeyIterator::KeyIterator(::rocksdb::Iterator* p_iter) : mp_iter{p_iter} {
    p_iter->SeekToFirst();
    m_key = deserialize_key(p_iter->key().ToStringView());
}

inline
bool KeyStore::KeyIterator::next_impl() {
    mp_iter->Next();
    if (mp_iter->Valid()) {
        m_key = deserialize_key(mp_iter->key().ToStringView());
        return true;
    }
    return false;
}

inline
KeyStore::ValueIterator::ValueIterator(::rocksdb::Iterator* p_iter) : mp_iter{p_iter} {
    p_iter->SeekToFirst();
    m_value = deserialize_value(p_iter->value().ToStringView());
}

inline
bool KeyStore::ValueIterator::next_impl() {
    mp_iter->Next();
    if (mp_iter->Valid()) {
        m_value = deserialize_value(mp_iter->value().ToStringView());
        return true;
    }
    return false;
}

inline
KeyStore::ItemIterator::ItemIterator(::rocksdb::Iterator* p_iter) : mp_iter{p_iter} {
    p_iter->SeekToFirst();
    m_item = std::make_pair(deserialize_key(p_iter->key().ToStringView()), deserialize_value(p_iter->value().ToStringView()));
}

inline
bool KeyStore::ItemIterator::next_impl() {
    mp_iter->Next();
    if (mp_iter->Valid()) {
        m_item = std::make_pair(deserialize_key(mp_iter->key().ToStringView()), deserialize_value(mp_iter->value().ToStringView()));
        return true;
    }
    return false;
}


inline
std::string KeyStore::serialize_key(const Key& key) {
    // TODO: move serialization out
    switch (key.type()) {
        case Key::NULL_I:  return "0";
        case Key::BOOL_I:  return key.as<bool>()? "2": "1";
        case Key::INT_I:   return '3' + key.to_str();
        case Key::UINT_I:  return '4' + key.to_str();
        case Key::FLOAT_I: return '5' + key.to_str();
        case Key::STR_I:   return '6' + key.to_str();
        default:           throw Key::wrong_type(key.type());
    }
}

inline
std::string KeyStore::serialize_value(const Object& value) {
    // TODO: move serialization out and parameterize
    switch (value.type()) {
        case Object::NULL_I:  return "0";
        case Object::BOOL_I:  return value.as<bool>()? "2": "1";
        case Object::INT_I:   return '3' + value.to_str();
        case Object::UINT_I:  return '4' + value.to_str();
        case Object::FLOAT_I: return '5' + value.to_str();  // TODO: provide perfect serialization for floats
        case Object::STR_I:   return '6' + value.as<String>();
        case Object::LIST_I:  [[fallthrough]];
        case Object::OMAP_I:  return '7' + value.to_json();
        default:      throw Object::wrong_type(value.type());
    }
}

inline
Key KeyStore::deserialize_key(const std::string_view& data) {
    // TODO: move serialization out and parameterize
    assert (data.size() >= 1);
    switch (data[0]) {
        case '0': return null;
        case '1': return false;
        case '2': return true;
        case '3': return str_to_int(data.substr(1));
        case '4': return str_to_uint(data.substr(1));
        case '5': return str_to_float(data.substr(1));  // TODO: provide perfect serialization for floats
        case '6': return data.substr(1);
        default:  ASSERT(false); // TODO: throw or return BAD key
    }
}

inline
Object KeyStore::deserialize_value(const std::string_view& data) {
    // TODO: move serialization out and parameterize
    assert (data.size() >= 1);
    switch (data[0]) {
        case '0': return null;
        case '1': return false;
        case '2': return true;
        case '3': return str_to_int(data.substr(1));
        case '4': return str_to_uint(data.substr(1));
        case '5': return str_to_float(data.substr(1));  // TODO: provide perfect serialization for floats
        case '6': return data.substr(1);
        case '7': return json::parse(data.substr(1));
        default:  ASSERT(false); // TODO: throw or return BAD object
    }
}

inline
void KeyStore::open(const std::filesystem::path& path, bool create_if_missing) {
    ASSERT(mp_db == nullptr);
    m_options.create_if_missing = create_if_missing;
    auto status = DBManager::get_instance().open(m_options, path.string(), mp_db);
    assert (status.ok());  // TODO: error handling
    m_open_path = path;
}

inline
Object KeyStore::read_key(const Object& target, const Key& key) {
    if (mp_db == nullptr) {
        open(m_path.empty()? nodel::filesystem::path(target): m_path, true);
    }
    auto db_key = serialize_key(key);
    std::string value;
    ::rocksdb::Status status = mp_db->Get(m_read_options, db_key, &value);
    if (status.code() == ::rocksdb::Status::Code::kNotFound)
        return null;
    ASSERT(status.ok()); // TODO: error handling
    return deserialize_value(value);
}

inline
void KeyStore::write_key(const Object& target, const Key& key, const Object& value, bool quiet) {
    updates.push_back(std::make_pair(key, value));
}

inline
void KeyStore::commit(const Object& target, const KeyList& del_keys, bool quiet) {
    if (mp_db == nullptr) {
        open(m_path.empty()? nodel::filesystem::path(target): m_path, true);
    }

    ::rocksdb::WriteBatch batch;

    // batch delete
    for (const auto& key : del_keys) {
        auto db_key = serialize_key(key);
        batch.Delete(db_key);
    }

    // batch update
    for (auto& item : updates) {
        auto db_key = serialize_key(item.first);
        auto db_value = serialize_value(item.second);
        batch.Put(db_key, db_value);
    }

    ::rocksdb::Status status = mp_db->Write(m_write_options, &batch);
    ASSERT(status.ok());  // TODO: error handling
}

} // nodel::rocksdb namespace

