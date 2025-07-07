/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include "Serializer.hxx"

#include <iostream>

constexpr size_t MAX_CHUNK_SIZE = 4096;

namespace nodel {

class StringSerializer : public Serializer
{
  public:
    StringSerializer() : Serializer{Object::STR} {}

    Object read(std::istream&, size_t size_hint) override;
    void write(std::ostream&, const Object&, const Object&) override;
};

inline
Object StringSerializer::read(std::istream& stream, size_t size_hint) {
    Object obj = Object::STR;
    auto& str = obj.as<String>();
    if (size_hint > 0) {
        str.resize(size_hint);
        stream.read(str.data(), size_hint);
    } else {
        size_t chunk_size = 1024;
        size_t read_pos = 0;
        str.resize(chunk_size);
        stream.read(str.data(), chunk_size);
        while (stream.good() && !stream.eof()) {
            read_pos += stream.gcount();
            if (chunk_size < MAX_CHUNK_SIZE) chunk_size <<= 1;
            str.resize(str.size() + chunk_size);
            stream.read(str.data() + read_pos, chunk_size);
        }
    }
    return obj;
}

inline
void StringSerializer::write(std::ostream& stream, const Object& obj, const Object&) {
    auto& str = obj.as<String>();
    stream.write(&str[0], str.size());
}

} // namespace nodel::serial
