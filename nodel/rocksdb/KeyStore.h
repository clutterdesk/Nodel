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
#include <nodel/core/serialize.h>
#include <nodel/filesystem/Directory.h>
#include <nodel/format/json.h>
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
    KeyStore(const std::filesystem::path& path, Options options, Origin origin);
    KeyStore(const std::filesystem::path& path, Options options) : KeyStore{path, options, Origin::MEMORY} {}
    KeyStore(const std::filesystem::path& path)                  : KeyStore{path, Options{}, Origin::MEMORY} {}
    KeyStore(Origin origin)                                      : KeyStore{{}, Options{}, origin} {}

    ~KeyStore();

    void set_db_options(::rocksdb::Options options)         { m_options = options; }
    void set_read_options(::rocksdb::ReadOptions options)   { m_read_options = options; }
    void set_write_options(::rocksdb::WriteOptions options) { m_write_options = options; }

    DataSource* new_instance(const Object& target, Origin origin) const override {
        return new KeyStore(std::filesystem::path{}, options(), origin);
    }

    void read_type(const Object& target) override { ASSERT(false); }
    void read(const Object& target) override {}  // TODO: implement
    void write(const Object&, const Object& cache) override {} // TODO: implement

    Object read_key(const Object&, const Key& k) override;
    void write_key(const Object&, const Key& k, const Object& v) override;
    void commit(const Object& target, const KeyList& del_keys) override;

    class KeyIterator : public nodel::DataSource::KeyIterator
    {
      public:
        KeyIterator(::rocksdb::Iterator* p_iter, const Interval& itvl);
        KeyIterator(::rocksdb::Iterator* p_iter);
        bool next_impl() override;
      private:
        ::rocksdb::Iterator* mp_iter;
        Interval m_itvl;
    };

    class ValueIterator : public nodel::DataSource::ValueIterator
    {
      public:
        ValueIterator(::rocksdb::Iterator* p_iter, const Interval& itvl);
        ValueIterator(::rocksdb::Iterator* p_iter);
        bool next_impl() override;
      private:
        ::rocksdb::Iterator* mp_iter;
        Interval m_itvl;
    };

    class ItemIterator : public nodel::DataSource::ItemIterator
    {
      public:
        ItemIterator(::rocksdb::Iterator* p_iter, const Interval& itvl);
        ItemIterator(::rocksdb::Iterator* p_iter);
        bool next_impl() override;
      private:
        ::rocksdb::Iterator* mp_iter;
        Interval m_itvl;
    };

    std::unique_ptr<nodel::DataSource::KeyIterator> key_iter() override;
    std::unique_ptr<nodel::DataSource::ValueIterator> value_iter() override;
    std::unique_ptr<nodel::DataSource::ItemIterator> item_iter() override;

    std::unique_ptr<nodel::DataSource::KeyIterator> key_iter(const Interval&) override;
    std::unique_ptr<nodel::DataSource::ValueIterator> value_iter(const Interval&) override;
    std::unique_ptr<nodel::DataSource::ItemIterator> item_iter(const Interval&) override;

  private:
    void open(const std::filesystem::path& path, bool create_if_missing);

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
KeyStore::KeyStore(const std::filesystem::path& path, Options options, Origin origin)
  : nodel::DataSource(Kind::SPARSE, options, Object::OMAP, origin)
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
KeyStore::KeyIterator::KeyIterator(::rocksdb::Iterator* p_iter, const Interval& itvl) : mp_iter{p_iter}, m_itvl{itvl} {
    ASSERT(deserialize(p_iter->key().ToStringView(), m_key));
}

inline
KeyStore::KeyIterator::KeyIterator(::rocksdb::Iterator* p_iter) : KeyIterator(p_iter, {}) {
}

inline
bool KeyStore::KeyIterator::next_impl() {
    mp_iter->Next();
    if (mp_iter->Valid()) {
        ASSERT(deserialize(mp_iter->key().ToStringView(), m_key));
        if (!m_itvl.is_empty() && m_itvl.contains(m_key))
            return false;
        return true;
    }
    // TODO: check status per API documentation to distinguish end from error
    return false;
}

inline
KeyStore::ValueIterator::ValueIterator(::rocksdb::Iterator* p_iter, const Interval& itvl) : mp_iter{p_iter}, m_itvl{itvl} {
    ASSERT(deserialize(p_iter->value().ToStringView(), m_value));
}

inline
KeyStore::ValueIterator::ValueIterator(::rocksdb::Iterator* p_iter) : ValueIterator(p_iter, {}) {
}

inline
bool KeyStore::ValueIterator::next_impl() {
    mp_iter->Next();
    if (mp_iter->Valid()) {
        Key key;
        ASSERT(deserialize(mp_iter->key().ToStringView(), key));
        if (!m_itvl.is_empty() && m_itvl.contains(key))
            return false;
        ASSERT(deserialize(mp_iter->value().ToStringView(), m_value));
        return true;
    }
    // TODO: check status per API documentation to distinguish end from error
    return false;
}

inline
KeyStore::ItemIterator::ItemIterator(::rocksdb::Iterator* p_iter, const Interval& itvl) : mp_iter{p_iter}, m_itvl{itvl} {
    Key key;
    ASSERT(deserialize(p_iter->key().ToStringView(), key));
    Object value;
    ASSERT(deserialize(p_iter->value().ToStringView(), value));
    m_item = std::make_pair(key, value);
}

inline
KeyStore::ItemIterator::ItemIterator(::rocksdb::Iterator* p_iter) : ItemIterator(p_iter, {}) {
}

inline
bool KeyStore::ItemIterator::next_impl() {
    mp_iter->Next();
    if (mp_iter->Valid()) {
        Key key;
        ASSERT(deserialize(mp_iter->key().ToStringView(), key));
        if (!m_itvl.is_empty() && m_itvl.contains(key))
            return false;
        Object value;
        ASSERT(deserialize(mp_iter->value().ToStringView(), value));
        m_item = std::make_pair(key, value);
        return true;
    }
    // TODO: check status per API documentation to distinguish end from error
    return false;
}

inline
std::unique_ptr<nodel::DataSource::KeyIterator> KeyStore::key_iter() {
    return key_iter({});
}

inline
std::unique_ptr<nodel::DataSource::ValueIterator> KeyStore::value_iter() {
    return value_iter({});
}

inline
std::unique_ptr<nodel::DataSource::ItemIterator> KeyStore::item_iter() {
    return item_iter({});
}

inline
std::unique_ptr<nodel::DataSource::KeyIterator> KeyStore::key_iter(const Interval& itvl) {
    auto p_it = mp_db->NewIterator(m_read_options);
    auto& min = itvl.min();
    if (min.value() == none) {
        p_it->SeekToFirst();
    } else {
        p_it->Seek(serialize(min.value()));
    }
    return std::make_unique<KeyIterator>(p_it, itvl);
}

inline
std::unique_ptr<nodel::DataSource::ValueIterator> KeyStore::value_iter(const Interval& itvl) {
    auto p_it = mp_db->NewIterator(m_read_options);
    auto& min = itvl.min();
    if (min.value() == none) {
        p_it->SeekToFirst();
    } else {
        p_it->Seek(serialize(min.value()));
    }
    return std::make_unique<ValueIterator>(p_it, itvl);
}

inline
std::unique_ptr<nodel::DataSource::ItemIterator> KeyStore::item_iter(const Interval& itvl) {
    auto p_it = mp_db->NewIterator(m_read_options);
    auto& min = itvl.min();
    if (min.value() == none) {
        p_it->SeekToFirst();
    } else {
        p_it->Seek(serialize(min.value()));
    }
    return std::make_unique<ItemIterator>(p_it, itvl);
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
    auto db_key = serialize(key);
    std::string data;
    ::rocksdb::Status status = mp_db->Get(m_read_options, db_key, &data);
    if (status.code() == ::rocksdb::Status::Code::kNotFound)
        return none;
    ASSERT(status.ok()); // TODO: error handling
    Object value;
    ASSERT(deserialize(data, value));
    return value;
}

inline
void KeyStore::write_key(const Object& target, const Key& key, const Object& value) {
    updates.push_back(std::make_pair(key, value));
}

inline
void KeyStore::commit(const Object& target, const KeyList& del_keys) {
    if (mp_db == nullptr) {
        open(m_path.empty()? nodel::filesystem::path(target): m_path, true);
    }

    ::rocksdb::WriteBatch batch;

    // batch delete
    for (const auto& key : del_keys) {
        auto db_key = serialize(key);
        batch.Delete(db_key);
    }

    // batch update
    for (auto& item : updates) {
        auto db_key = serialize(item.first);
        auto db_value = serialize(item.second);
        batch.Put(db_key, db_value);
    }

    ::rocksdb::Status status = mp_db->Write(m_write_options, &batch);
    ASSERT(status.ok());  // TODO: error handling
}

} // nodel::rocksdb namespace

