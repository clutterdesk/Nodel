#pragma once

#include <nodel/support/Ref.h>
#include <nodel/core/Object.h>

#include <filesystem>
#include <functional>

namespace nodel::filesystem {

/////////////////////////////////////////////////////////////////////////////
/// A registry that maps filesystem extensions to DataSources.
/// - In general the schema of a file cannot be determined by examining its content. The Registry
///   provides a means of determining schema from the filesystem path of the file.
/// - If no rule applies for a given file, or directory, the default factory is used, if present.
///   Separate default factories are provided for files and directories.
/// - If no rule applies, and no default factory is defined, no object will be created to represent
///   the path. Such elided files/directories will not be removed when the parent object is saved.
/// - A default factory can further differentiate files or directories based on the full path.
/////////////////////////////////////////////////////////////////////////////
class Registry
{
  public:
    using Origin = DataSource::Origin;
    using Factory = std::function<DataSource*(const Object&, const std::filesystem::path&, Origin)>;

    void set_file_default(const Factory& factory);
    void set_directory_default(const Factory& factory);
    void associate(const String& ext, const Factory& factory);

    template <class T> void set_file_default();
    template <class T> void set_directory_default();
    template <class T> void associate(const String& extension);

    Factory get_association(const std::filesystem::path& path) const;

    DataSource* create(const Object&, const std::filesystem::path&, Origin) const;
    DataSource* create(const Object&, const std::filesystem::path&, Origin, bool is_directory) const;

    bool is_empty() const;

  protected:
    std::unordered_map<String, Factory> m_ext_map;
    Factory m_dir_default;
    Factory m_file_default;

  private:
    refcnt_t m_ref_count;

  template <typename> friend class ::nodel::Ref;
};


inline
void Registry::set_file_default(const Factory& factory) {
    m_file_default = factory;
}

inline
void Registry::set_directory_default(const Factory& factory) {
    m_dir_default = factory;
}

inline
void Registry::associate(const String& ext, const Factory& factory) {
    m_ext_map[ext] = factory;
}

template <class T>
void Registry::set_file_default() {
    m_file_default = [] (const Object&, const std::filesystem::path&, Origin origin) { return new T(origin); };
}

template <class T>
void Registry::set_directory_default() {
    m_dir_default = [] (const Object&, const std::filesystem::path&, Origin origin) { return new T(origin); };
}

template <class T>
void Registry::associate(const String& ext) {
    m_ext_map[ext] = [] (const Object&, const std::filesystem::path&, Origin origin) { return new T(origin); };
}

inline
Registry::Factory Registry::get_association(const std::filesystem::path& path) const {
    auto ext = path.extension().string();
    if (auto it = m_ext_map.find(ext); it != m_ext_map.end()) {
        return it->second;
    }
    return {};
}

inline
DataSource* Registry::create(const Object& target, const std::filesystem::path& path, Origin origin) const {
    auto ext = path.extension().string();
    if (auto it = m_ext_map.find(ext); it != m_ext_map.end()) {
        return it->second(target, path, origin);
    }
    return nullptr;
}

inline
DataSource* Registry::create(const Object& target, const std::filesystem::path& path, Origin origin, bool is_directory) const {
    auto ext = path.extension().string();
    if (auto it = m_ext_map.find(ext); it != m_ext_map.end()) {
        return it->second(target, path, origin);
    }
    return is_directory? m_dir_default(target, path, origin): m_file_default(target, path, origin);
}

inline
bool Registry::is_empty() const {
    return !m_file_default && !m_dir_default && m_ext_map.size() == 0;
}

} // namespace nodel::filesystem
