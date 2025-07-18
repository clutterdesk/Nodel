/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include "Serializer.hxx"

#include <nodel/support/parse.hxx>
#include <nodel/parser/json.hxx>

#include <iostream>

namespace nodel {

class JsonSerializer : public Serializer
{
  public:
    Object read(std::istream&, size_t size_hint) override;
    void write(std::ostream&, const Object&, const Object& options) override;
};

inline
Object JsonSerializer::read(std::istream& stream, size_t size_hint) {
    json::impl::Parser parser{parse::StreamAdapter{stream}};
    parser.parse_object();
    return parser.m_curr;
}

inline
void JsonSerializer::write(std::ostream& stream, const Object& obj, const Object& options) {
    auto indent = options.get_if("indent"_key, Object{0}).cast<Int>();
    obj.to_json(stream, (int)indent);
}

} // namespace nodel::serial
