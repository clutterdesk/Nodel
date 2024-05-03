#pragma once

#include <nodel/filesystem/Directory.h>
#include <nodel/filesystem/JsonFile.h>
#include <nodel/filesystem/CsvFile.h>
#include <nodel/filesystem/GenericFile.h>

namespace nodel::filesystem {

namespace impl {

/////////////////////////////////////////////////////////////////////////////
/// @brief Filesystem initialization macro.
/// This macro must be instantiated before using nodel::filesystem services.
/// It defines thread-local data-structures.
/////////////////////////////////////////////////////////////////////////////
#define NODEL_INIT_FILESYSTEM namespace nodel::filesystem::impl { thread_local Registry default_registry; }
extern thread_local Registry default_registry;

inline
void init_default_registry() {
    default_registry.set_directory_default<SubDirectory>();
    default_registry.set_file_default<GenericFile>();
    default_registry.associate<JsonFile>(".json");
    default_registry.associate<CsvFile>(".csv");
    default_registry.associate<GenericFile>(".txt");
}

} // namespace impl

/////////////////////////////////////////////////////////////////////////////
/// @brief Returns the *thread-local* default registry.
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
/// @brief Configure Nodel filesystem.
/// - Enable binding URI with the "file" scheme using
///   nodel::bind(const String& uri_spec, ...).
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
