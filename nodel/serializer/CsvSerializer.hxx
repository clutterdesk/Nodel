/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include "Serializer.hxx"

#include <nodel/parser/csv.hxx>
#include <nodel/core/Object.hxx>

#include <iostream>


namespace nodel {

class CsvSerializer : public Serializer
{
  public:
    CsvSerializer() : Serializer{Object::LIST} {}

    Object read(std::istream&, size_t size_hint) override;
    void write(std::ostream&, const Object&) override;
};

inline
Object CsvSerializer::read(std::istream& stream, size_t size_hint) {
    csv::impl::Parser parser{stream};
    return parser.parse();
}

inline
void CsvSerializer::write(std::ostream& stream, const Object& obj) {
    for (const auto& row : obj.iter_values()) {
        char sep = 0;
        for (const auto& col: row.iter_values()) {
            if (sep == 0) sep = ','; else stream << sep;
            stream << col.to_json();
        }
        stream << "\n";
    }
}

} // namespace nodel::serial
