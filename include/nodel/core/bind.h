/** @file */
#pragma once

#include <nodel/core/Object.h>
#include <nodel/core/uri.h>
#include <nodel/support/exception.h>
#include <sstream>

namespace nodel {

struct BindError : public NodelException
{
    BindError(std::string&& err) : NodelException{std::forward<std::string>(err)} {}
};

/////////////////////////////////////////////////////////////////////////////
/// @brief Bind an Object to the specified external storage location.
/// @param uri A URI whose scheme identifies a registered DataSource.
/// @param obj The Object to bind.
/// - This form of the `bind` function associates data in memory with some
///   external storage location. After calling this function, the data can
///   be written to the storage location by calling Object::save().
/// - As with Object::set(const Key&, const Object&), if @p obj has a parent,
///   then a copy of the Object is bound, and the copy is returned.
/// - The URI scheme must be a registered scheme. For example, the `file:`
///   scheme is registered by calling
///   nodel::filesystem::configure(nodel::DataSource::Options).
/// @return Returns @p obj or a copy of it.
/////////////////////////////////////////////////////////////////////////////
inline
Object bind(const URI& uri, const Object& obj) {
    Object bound = obj;
    auto scheme = uri.get("scheme"_key).as<String>();
    auto factory = lookup_uri_scheme(scheme);
    if (factory) {
        auto p_ds = factory(uri);
        if (obj == nil) bound = p_ds; else p_ds->bind(bound);
        p_ds->configure(uri);
        return bound;
    } else {
        std::stringstream ss;
        ss << "URI scheme not found: " << scheme;
        throw BindError(ss.str());
    }
}

/////////////////////////////////////////////////////////////////////////////
/// @brief Bind a new Object to the specified URI.
/// @param uri A URI whose scheme identifies a registered DataSource.
/// - This form of the `bind` function should be used when data already
///   exists in the external storage location pointed to by the URI.  After
///   calling this function, the data will be loaded on-demand when an
///   accessor function of the nodel::Object class is called.
/// - The URI scheme must be a registered scheme. For example, the `file:`
///   scheme is registered by calling
///   nodel::filesystem::configure(nodel::DataSource::Options).
/// @return Returns an Object bound to the specified URI.
/////////////////////////////////////////////////////////////////////////////
inline
Object bind(const URI& uri) {
    return bind(uri, nil);
}

/////////////////////////////////////////////////////////////////////////////
/// @brief Bind an Object to a specific DataSource.
/// @tparam DataSourceType A concrete DataSource sub-class.
/// @param obj The Object to bind.
/// - This form of the `bind` function binds a specific DataSource to an
///   Object. After calling this function, the data can be written to the
///   external storage location by calling Object::save().
/// @return Returns @p obj or a copy of it.
/////////////////////////////////////////////////////////////////////////////
template <class DataSourceType>
Object bind(const Object& obj) {
    static_assert (std::is_base_of<DataSource, DataSourceType>::value);
    auto p_ds = new DataSourceType(DataSource::Origin::MEMORY);
    Object bound = obj;
    p_ds->bind(bound);
    return bound;
}

/////////////////////////////////////////////////////////////////////////////
/// @brief Bind an Object to a specific DataSource.
/// @tparam DataSourceType A concrete DataSource sub-class.
/// - This form of the `bind` function should be used when data already
///   exists in the external storage location pointed to by the URI.  After
///   calling this function, the data will be loaded on-demand when an
///   accessor function of the nodel::Object class is called.
/// @return Returns an Object bound to the specified URI.
/////////////////////////////////////////////////////////////////////////////
template <class DataSourceType>
Object bind() {
    static_assert (std::is_base_of<DataSource, DataSourceType>::value);
    return new DataSourceType(DataSource::Origin::SOURCE);
}

} // namespace nodel
