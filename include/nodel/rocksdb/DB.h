/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Object.h>
#include <nodel/core/serialize.h>
#include <nodel/filesystem/Directory.h>
#include <nodel/parser/json.h>

#include "Comparator.h"
#include "DBManager.h"

#include <rocksdb/db.h>
#include <filesystem>

namespace nodel::rocksdb {

/////////////////////////////////////////////////////////////////////////////
/// A simple key/value store backed by RocksDB.
// - Multiple objects may operate on the same DB, but no synchronization is provided.
//   Use Object::refresh or Object::reset to synchronize one object after changes have
//   been made to the DB.
// - Updates and deletes are batched together when Object::save is called, providing
//   atomicity.
/////////////////////////////////////////////////////////////////////////////
class DB : public nodel::DataSource
{
  public:
    DB(const std::filesystem::path& path, Origin origin);
    DB(DataSource::Origin origin) : DB({}, origin) {}
    ~DB();

    DB(const DB&) = delete;
    DB(DB&&) = delete;
    DB* operator = (const DB&) = delete;
    DB* operator = (DB&&) = delete;

    void set_db_options(::rocksdb::Options options)         { m_options = options; }
    void set_read_options(::rocksdb::ReadOptions options)   { m_read_options = options; }
    void set_write_options(::rocksdb::WriteOptions options) { m_write_options = options; }

    DataSource* new_instance(const Object& target, Origin origin) const override {
        return new DB(std::filesystem::path{}, origin);
    }

    void configure(const Object& uri) override;

    void read_type(const Object& target) override { ASSERT(false); }
    void read(const Object& target) override;
    void write(const Object&, const Object& cache) override;

    Object read_key(const Object&, const Key& k) override;
    void write_key(const Object&, const Key& k, const Object& v) override;
    void commit(const Object& target, const Object& data, const KeyList& del_keys) override;

    class KeyIterator : public nodel::DataSource::KeyIterator
    {
      public:
        KeyIterator(DB* p_db, ::rocksdb::Iterator* p_iter, const Slice& slice);
        bool next_impl() override;
        bool read_key();
      private:
        DB* mp_ds;
        ::rocksdb::Iterator* mp_iter;
        Slice m_slice;
    };

    class ValueIterator : public nodel::DataSource::ValueIterator
    {
      public:
        ValueIterator(DB* p_db, ::rocksdb::Iterator* p_iter, const Slice& slice);
        bool next_impl() override;
        bool read_value();
      private:
        DB* mp_ds;
        ::rocksdb::Iterator* mp_iter;
        Slice m_slice;
    };

    class ItemIterator : public nodel::DataSource::ItemIterator
    {
      public:
        ItemIterator(DB* p_db, ::rocksdb::Iterator* p_iter, const Slice& slice);
        bool next_impl() override;
        bool read_item();
      private:
        DB* mp_ds;
        ::rocksdb::Iterator* mp_iter;
        Slice m_slice;
    };

    std::unique_ptr<nodel::DataSource::KeyIterator> key_iter() override;
    std::unique_ptr<nodel::DataSource::ValueIterator> value_iter() override;
    std::unique_ptr<nodel::DataSource::ItemIterator> item_iter() override;

    std::unique_ptr<nodel::DataSource::KeyIterator> key_iter(const Slice&) override;
    std::unique_ptr<nodel::DataSource::ValueIterator> value_iter(const Slice&) override;
    std::unique_ptr<nodel::DataSource::ItemIterator> item_iter(const Slice&) override;

  private:
    void open(const std::filesystem::path& path, bool create_if_missing);

  private:
    std::filesystem::path m_path;
    std::filesystem::path m_open_path;
    ::rocksdb::DB* mp_db = nullptr;
    ::rocksdb::Options m_options;
    ::rocksdb::ReadOptions m_read_options;
    ::rocksdb::WriteOptions m_write_options;
    ItemList m_updates;
    bool m_update_all = false;
};


inline
DB::DB(const std::filesystem::path& path, Origin origin)
  : nodel::DataSource(Kind::SPARSE, Object::OMAP, origin)
  , m_path{path} {
    m_options.error_if_exists = false;
    if (!path.empty()) open(path, true);
}

inline
DB::~DB() {
    if (mp_db != nullptr) {
        DBManager::get_instance().close(m_open_path);
        mp_db = nullptr;
    }
}


inline
DB::KeyIterator::KeyIterator(DB* p_ds, ::rocksdb::Iterator* p_iter, const Slice& slice)
: mp_ds{p_ds}, mp_iter{p_iter}, m_slice{slice}
{
    if (!read_key()) m_key = nil;
}

inline
bool DB::KeyIterator::next_impl() {
    mp_iter->Next();
    return read_key();
}

inline
bool DB::KeyIterator::read_key() {
    if (mp_iter->Valid()) {
        ASSERT(deserialize(mp_iter->key().ToStringView(), m_key));
        if (!m_slice.is_empty() && !m_slice.contains(m_key))
            return false;
        return true;
    }
    if (!mp_iter->status().ok()) {
        mp_ds->report_read_error(mp_iter->status().ToString());
    }
    return false;
}

inline
DB::ValueIterator::ValueIterator(DB* p_ds, ::rocksdb::Iterator* p_iter, const Slice& slice)
: mp_ds{p_ds}, mp_iter{p_iter}, m_slice{slice}
{
    if (!read_value()) m_value.release();
}

inline
bool DB::ValueIterator::next_impl() {
    mp_iter->Next();
    return read_value();
}

inline
bool DB::ValueIterator::read_value() {
    if (mp_iter->Valid()) {
        Key key;
        ASSERT(deserialize(mp_iter->key().ToStringView(), key));
        if (!m_slice.is_empty() && !m_slice.contains(key))
            return false;
        ASSERT(deserialize(mp_iter->value().ToStringView(), m_value));
        return true;
    }
    if (!mp_iter->status().ok()) {
        mp_ds->report_read_error(mp_iter->status().ToString());
    }
    return false;
}

inline
DB::ItemIterator::ItemIterator(DB* p_ds, ::rocksdb::Iterator* p_iter, const Slice& slice)
: mp_ds{p_ds}, mp_iter{p_iter}, m_slice{slice}
{
    if (!read_item()) m_item.first = nil;
}

inline
bool DB::ItemIterator::next_impl() {
    mp_iter->Next();
    return read_item();
}

inline
bool DB::ItemIterator::read_item() {
    if (mp_iter->Valid()) {
        Key key;
        ASSERT(deserialize(mp_iter->key().ToStringView(), key));
        if (!m_slice.is_empty() && !m_slice.contains(key))
            return false;
        Object value;
        ASSERT(deserialize(mp_iter->value().ToStringView(), value));
        m_item.first = key;
        m_item.second = value;
        return true;
    }
    if (!mp_iter->status().ok()) {
        mp_ds->report_read_error(mp_iter->status().ToString());
    }
    return false;
}

inline
std::unique_ptr<nodel::DataSource::KeyIterator> DB::key_iter() {
    return key_iter({});
}

inline
std::unique_ptr<nodel::DataSource::ValueIterator> DB::value_iter() {
    return value_iter({});
}

inline
std::unique_ptr<nodel::DataSource::ItemIterator> DB::item_iter() {
    return item_iter({});
}

inline
std::unique_ptr<nodel::DataSource::KeyIterator> DB::key_iter(const Slice& slice) {
    if (mp_db == nullptr) {
        assert (!m_path.empty());
        open(m_path, true);
    }

    auto p_it = mp_db->NewIterator(m_read_options);
    auto& min = slice.min();
    if (min.value() == nil) {
        p_it->SeekToFirst();
    } else {
        p_it->Seek(serialize(min.value()));
    }
    if (!p_it->status().ok()) {
        report_read_error(p_it->status().ToString());
    }
    return std::make_unique<KeyIterator>(this, p_it, slice);
}

inline
std::unique_ptr<nodel::DataSource::ValueIterator> DB::value_iter(const Slice& slice) {
    if (mp_db == nullptr) {
        assert (!m_path.empty());
        open(m_path, true);
    }

    auto p_it = mp_db->NewIterator(m_read_options);
    auto& min = slice.min();
    if (min.value() == nil) {
        p_it->SeekToFirst();
    } else {
        p_it->Seek(serialize(min.value()));
    }
    if (!p_it->status().ok()) {
        report_read_error(p_it->status().ToString());
    }
    return std::make_unique<ValueIterator>(this, p_it, slice);
}

inline
std::unique_ptr<nodel::DataSource::ItemIterator> DB::item_iter(const Slice& slice) {
    if (mp_db == nullptr) {
        assert (!m_path.empty());
        open(m_path, true);
    }

    auto p_it = mp_db->NewIterator(m_read_options);
    auto& min = slice.min();
    if (min.value() == nil) {
        p_it->SeekToFirst();
    } else {
        p_it->Seek(serialize(min.value()));
    }
    if (!p_it->status().ok()) {
        report_read_error(p_it->status().ToString());
    }
    return std::make_unique<ItemIterator>(this, p_it, slice);
}

inline
void DB::open(const std::filesystem::path& path, bool create_if_missing) {
    ASSERT(mp_db == nullptr);
    m_options.create_if_missing = create_if_missing;
    m_options.comparator = new nodel::rocksdb::Comparator();
    auto status = DBManager::get_instance().open(m_options, path.string(), mp_db);
    if (!status.ok()) {
        report_read_error(status.ToString());
    }
    m_open_path = path;
}

inline
void DB::configure(const Object& uri) {
    DataSource::configure(uri);

    auto path = uri.get("path"_key);
    if (path == nil || path.size() == 0)
        path = uri.get("query.path"_path);
    m_path = path.to_str();
}

inline
void DB::read(const Object& target) {
    if (mp_db == nullptr)
        open(m_path.empty()? nodel::filesystem::path(target): m_path, true);

    auto p_it = mp_db->NewIterator(m_read_options);
    if (p_it != nullptr) {
        for (p_it->SeekToFirst(); p_it->Valid(); p_it->Next()) {
            Key key;
            ASSERT(deserialize(p_it->key().ToStringView(), key));
            Object value;
            ASSERT(deserialize(p_it->value().ToStringView(), value));
            read_set(target, key, value);
        }

        if (!p_it->status().ok()) {
            report_read_error(p_it->status().ToString());
        }

        delete p_it;
    }
}

inline
void DB::write(const Object& target, const Object& data) {
    m_update_all = true;
}

inline
Object DB::read_key(const Object& target, const Key& key) {
    if (mp_db == nullptr)
        open(m_path.empty()? nodel::filesystem::path(target): m_path, true);

    auto db_key = serialize(key);
    std::string data;
    ::rocksdb::Status status = mp_db->Get(m_read_options, db_key, &data);
    if (status.code() == ::rocksdb::Status::Code::kNotFound)
        return nil;

    if (!status.ok()) {
        report_read_error(status.ToString());
        return nil;
    } else {
        Object value;
        ASSERT(deserialize(data, value));
        return value;
    }
}

inline
void DB::write_key(const Object& target, const Key& key, const Object& value) {
    m_updates.push_back(std::make_pair(key, value));
}

inline
void DB::commit(const Object& target, const Object& data, const KeyList& del_keys) {
    if (mp_db == nullptr)
        open(m_path.empty()? nodel::filesystem::path(target): m_path, true);

    ::rocksdb::WriteBatch batch;

    // batch delete
    for (const auto& key : del_keys) {
        auto db_key = serialize(key);
        batch.Delete(db_key);
    }

    // batch update
    if (m_update_all) {
        for (auto& item : data.iter_items()) {
            auto db_key = serialize(item.first);
            auto db_value = serialize(item.second);
            batch.Put(db_key, db_value);
        }
    } else {
        for (auto& item : m_updates) {
            auto db_key = serialize(item.first);
            auto db_value = serialize(item.second);
            batch.Put(db_key, db_value);
        }
    }

    ::rocksdb::Status status = mp_db->Write(m_write_options, &batch);
    if (!status.ok()) {
        report_write_error(status.ToString());
    }

    m_updates.clear();
    m_update_all = false;
}

} // nodel::rocksdb namespace

