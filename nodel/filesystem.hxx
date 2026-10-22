/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/filesystem/Directory.hxx>
#include <nodel/filesystem/SerialFile.hxx>
#include <nodel/serializer/CsvSerializer.hxx>
#include <nodel/serializer/JsonSerializer.hxx>
#include <nodel/serializer/StringSerializer.hxx>
#include <nodel/filesystem/ZipFile.hxx>

namespace nodel::filesystem {

namespace impl {

inline
Registry& default_registry() {
    static thread_local Registry registry;
    return registry;
}

inline
void init_default_registry(Registry& registry) {
    registry.set_directory_default<SubDirectory>();
    registry.set_file_default(new StringSerializer());
    registry.associate(".json", new JsonSerializer());
    registry.associate(".csv", new CsvSerializer());
    registry.associate(".txt", new StringSerializer());
    registry.associate<ZipFile>(".zip");
}

} // namespace impl

/////////////////////////////////////////////////////////////////////////////
/// Deprecated: no longer required, expands to nothing.
/////////////////////////////////////////////////////////////////////////////
#define NODEL_INIT_FILESYSTEM

/////////////////////////////////////////////////////////////////////////////
/// Returns the *thread-local* default registry.
/// Changes to the file associations in this registry will affect *all*
/// Objects bound to the filesystem in the current thread.
/////////////////////////////////////////////////////////////////////////////
inline
Registry& default_registry() {
    auto& registry = impl::default_registry();
    if (registry.is_empty()) {
        impl::init_default_registry(registry);
    }
    return registry;
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
