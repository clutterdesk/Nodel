/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/filesystem/File.hxx>

#include <nodel/core/Object.hxx>
#include <nodel/support/Ref.hxx>
#include <nodel/serializer/Serializer.hxx>

#include <fstream>


namespace nodel::filesystem {

/////////////////////////////////////////////////////////////////////////////
/// Base class for all file DataSource classes
/////////////////////////////////////////////////////////////////////////////
class SerialFile : public File
{
  public:
    SerialFile(const Ref<Serializer>& r_serial, Origin origin)
    : File(DataSource::Kind::COMPLETE, r_serial->get_repr_ix(), origin)
    , mr_serial{r_serial} {}

    SerialFile(const Ref<Serializer>& r_serial) : SerialFile(r_serial, Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override {
        return new SerialFile(mr_serial, origin);
    }

    void read(const Object& target) override;
    void write(const Object& target, const Object& cache, const Object& options) override;

  private:
    Ref<Serializer> mr_serial;
};

} // namespace nodel::filesystem
