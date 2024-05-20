/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/filesystem/Directory.h>
#include <nodel/filesystem/SerialFile.h>
#include <nodel/serializer/CsvSerializer.h>
#include <nodel/serializer/JsonSerializer.h>
#include <nodel/serializer/StringSerializer.h>
#include <nodel/filesystem/ZipFile.h>

namespace nodel::filesystem {

namespace impl {

/////////////////////////////////////////////////////////////////////////////
/// Filesystem initialization macro.
/// This macro must be instantiated before using nodel::filesystem services.
/// It defines thread-local data-structures.
/////////////////////////////////////////////////////////////////////////////
#define NODEL_INIT_FILESYSTEM namespace nodel::filesystem::impl { thread_local Registry default_registry; }
extern thread_local Registry default_registry;

inline
void init_default_registry() {
    default_registry.set_directory_default<SubDirectory>();
    default_registry.set_file_default(new StringSerializer());
    default_registry.associate(".json", new JsonSerializer());
    default_registry.associate(".csv", new CsvSerializer());
    default_registry.associate(".txt", new StringSerializer());
    default_registry.associate<ZipFile>(".zip");
}

} // namespace impl

/////////////////////////////////////////////////////////////////////////////
/// Returns the *thread-local* default registry.
/// Changes to the file associations in this registry will affect *all*
/// Objects bound to the filesystem in the current thread.
/////////////////////////////////////////////////////////////////////////////
inline
Registry& default_registry() {
    if (impl::default_registry.is_empty()) {
        impl::init_default_registry();
    }
    return impl::default_registry;
}

/////////////////////////////////////////////////////////////////////////////
/// Configure Nodel filesystem.
/// - Enable binding URI with the "file" scheme using
///   nodel::bind(const String& uri_spec, ...).
/// - By default, Objects bound to the filesystem have their DataSource mode
///   set to read-only. See @ref DataSource::Options for more information.
/// - Use URI query to set path when the path is relative
///   (ex: file://?path=a/b).
/////////////////////////////////////////////////////////////////////////////
inline
void configure(DataSource::Options default_options={}) {
    register_uri_scheme("file", [default_options] (const Object& uri, DataSource::Origin origin) -> DataSource* {
        auto p_ds = new Directory(new Registry{default_registry()}, origin);
        p_ds->set_options(default_options);
        return p_ds;
    });
}

} // namespace nodel::filesystem
