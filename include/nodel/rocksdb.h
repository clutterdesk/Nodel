#pragma once

#include <nodel/rocksdb/DB.h>
#include <nodel/filesystem.h>
#include <nodel/support/logging.h>

using namespace nodel;

namespace nodel::rocksdb {

struct Options
{
    void configure(const URI& uri) {
        base.configure(uri);
        // TODO: rocksdb configuration from URI
    }

    DataSource::Options base;
    ::rocksdb::Options db;
    ::rocksdb::ReadOptions db_read;
    ::rocksdb::ReadOptions db_write;
    std::string db_ext = ".rocksdb";
};

/**
 * @brief Enable and configure the URI "rocksdb" scheme.
 * - Enable binding URI with the "rocksdb" scheme using nodel::bind(const String& uri_spec, ...).
 * - Create directory association for ".rocksdb" in the default registry.
 * @see nodel::bind(const String&, Object)
 */
inline
void configure(Options options={}) {
    register_uri_scheme("rocksdb", [&options] (const Object& uri) -> DataSource* {
        options.configure(uri);
        auto p_ds = new rocksdb::DB(uri.get("path"_key).to_str(), options.base, DataSource::Origin::SOURCE);
        p_ds->set_db_options(options.db);
        p_ds->set_db_read_options(options.db_read);
        p_ds->set_db_write_options(options.db_write);
        return p_ds;
    });

    default_registry().associate<rocksdb::DB>(options.db_ext);
}

/**
 * @brief Register a directory extension to recognize RocksDB database directories.
 * This function can be used to customize the extension used in a single tree, when that
 * extension is different from the default extension.
 * @param fs_obj A filesystem object.
 * @param ext The directory extension, including the leading period.
 * @note The extension is registered for the entire tree.
 */
inline
void register_directory_extension(const Object& fs_obj, const std::string& ext = ".rocksdb") {
    Ref<Registry> r_reg = filesystem::get_registry(dir);
    if (!r_reg) {
        WARN("Argument must be a filesystem directory object.");
        return;
    }
    r_reg->associate<rocksdb::DB>(ext);
}

} // namespace nodel::rocksdb

