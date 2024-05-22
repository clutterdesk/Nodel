/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/kvdb/DB.hxx>
#include <nodel/filesystem.hxx>
#include <nodel/support/logging.hxx>

using namespace nodel;
using Registry = filesystem::Registry;

namespace nodel::kvdb {

struct Options
{
    void configure(const URI& uri) {
        base.configure(uri);
        // TODO: rocksdb configuration from URI
    }

    DataSource::Options base;
    ::rocksdb::Options db;
    ::rocksdb::ReadOptions db_read;
    ::rocksdb::WriteOptions db_write;
    std::string db_ext = ".kvdb";
};

/////////////////////////////////////////////////////////////////////////////
/// Enable and configure the URI "kvdb" scheme.
/// - Enable binding URI with the "kvdb" scheme using nodel::bind(const String& uri_spec, ...).
/// - Create directory association for ".kvdb" in the default registry.
/// @see nodel::bind(const String&, Object)
/////////////////////////////////////////////////////////////////////////////
inline
void configure(Options options={}) {
    register_uri_scheme("kvdb", [options] (const URI& uri, DataSource::Origin origin) -> DataSource* {
        auto p_ds = new kvdb::DB(origin);
        p_ds->set_options(options.base);
        p_ds->set_db_options(options.db);
        p_ds->set_read_options(options.db_read);
        p_ds->set_write_options(options.db_write);
        return p_ds;
    });

    filesystem::default_registry().associate<kvdb::DB>(options.db_ext);
}

/////////////////////////////////////////////////////////////////////////////
/// Register a directory extension to recognize RocksDB database directories.
/// This function can be used to customize the extension used in a single tree, when that
/// extension is different from the default extension.
/// @param fs_obj A filesystem object.
/// @param ext The directory extension, including the leading period.
/// @note The extension is registered for the entire tree.
/////////////////////////////////////////////////////////////////////////////
inline
void register_directory_extension(const Object& fs_obj, const std::string& ext = ".kvdb") {
    Ref<Registry> r_reg = filesystem::get_registry(fs_obj);
    if (!r_reg) {
        WARN("Argument must be a filesystem directory object.");
        return;
    }
    r_reg->associate<kvdb::DB>(ext);
}

} // namespace nodel::kvdb

