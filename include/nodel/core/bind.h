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

inline
Object bind(const URI& uri) {
    return bind(uri, nil);
}

template <class DataSourceType>
Object bind(const Object& obj) {
    static_assert (std::is_base_of<DataSource, DataSourceType>::value);
    auto p_ds = new DataSourceType(DataSource::Origin::MEMORY);
    Object bound = obj;
    p_ds->bind(bound);
    return bound;
}

} // namespace nodel
