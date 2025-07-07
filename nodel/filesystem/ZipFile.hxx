/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Object.hxx>
#include <nodel/filesystem/Directory.hxx>
#include <nodel/support/Finally.hxx>

#include <queue>
#include <ZipLib/ZipFile.h>

namespace nodel {
namespace filesystem {

class ZipFile : public File
{
  public:
    ZipFile(Origin origin) : File(Kind::COMPLETE, Object::OMAP, origin, Multilevel::YES) { set_mode(mode() | Mode::INHERIT); }
    ZipFile()              : ZipFile(Origin::MEMORY) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new ZipFile(origin); }

    void read(const Object& target) override;
    void write(const Object& target, const Object& cache, const Object&) override;

  protected:
    void free_resources() override;

  private:
    ZipArchive::Ptr mp_zip;
};


namespace impl {

class ZipFileEntry : public File
{
  public:
    ZipFileEntry(const ZipArchiveEntry::Ptr& p_entry, Origin origin)
    : File(Kind::COMPLETE, origin, Multilevel::NO), mp_entry{p_entry}
    { set_mode(mode() | Mode::INHERIT); }

    ZipFileEntry(const ZipArchiveEntry::Ptr& p_entry) : ZipFileEntry(p_entry, DataSource::Origin::SOURCE) {}

    DataSource* new_instance(const Object& target, Origin origin) const override { return new ZipFileEntry(mp_entry, origin); }

    void read(const Object& target) override;
    void write(const Object& target, const Object& cache, const Object&) override {}

  private:
    ZipArchiveEntry::Ptr mp_entry;

  friend class nodel::filesystem::ZipFile;
};

}  // namespace impl


inline
void ZipFile::read(const Object& target) {
    ASSERT(!mp_zip);

    auto r_reg = get_registry(target);
    auto fpath = path(target);

    try {
        mp_zip = ::ZipFile::Open(fpath);
    } catch (std::exception& exc) {
        throw NodelException(exc.what());
    }

    OrderedMap zip_map;
    auto entries = mp_zip->GetEntriesCount();
    for (size_t i=0; i<entries; ++i)
    {
        auto p_entry = mp_zip->GetEntry(int(i));
        if (!p_entry->IsDirectory()) {
            std::filesystem::path fpath{p_entry->GetFullName()};
            auto r_ser = r_reg->get_serializer(fpath);
            if (r_ser && p_entry->CanExtract()) {
                auto p_ds = new impl::ZipFileEntry(p_entry);
                p_ds->set_options(options());

                KeyList keys;
                Key first_key;
                size_t k = 0;
                for (const auto& felem : fpath) {
                    if (k++ == 0) first_key = felem.string(); else keys.push_back(felem.string());
                }

                if (k > 0) {
                    if (k == 1) {
                        zip_map[first_key] = p_ds;
                    } else if (k == 2) {
                        Object& child = zip_map[first_key];
                        if (child.is_empty()) child.set(Object::OMAP);
                        zip_map[first_key].set(keys[0], p_ds);
                    } else {
                        Object& child = zip_map[first_key];
                        if (child.is_empty()) child.set(Object::OMAP);
                        zip_map[first_key].set(OPath{keys}, p_ds);
                    }
                }
            }
        }
    }

    for (auto& item : zip_map) {
        read_set(target, item.first, item.second);
    }
}

inline
void ZipFile::write(const Object& target, const Object& cache, const Object& save_options) {
    auto fpath = path(target);
    auto r_reg = get_registry(target);

    std::vector<std::stringstream*> streams;
    Finally finally{ [&streams] () {
        for (auto p_stream : streams)
            delete p_stream;
    }};

    // create new archive
    free_resources();
    mp_zip = ZipArchive::Create();

    // prime stack
    std::queue<std::pair<OPath, Object>> fifo;
    for (auto& child: target.iter_values())
        fifo.push(std::make_pair(child.path(target), child));

    // save entries
    while (!fifo.empty()) {
        auto item = fifo.front();
        auto& rel_path = item.first;
        auto obj = item.second;
        fifo.pop();

        std::filesystem::path rel_fpath;
        for (const auto& key : rel_path)
            rel_fpath /= key.to_str();

        auto item_fpath = fpath / rel_fpath;

        auto p_ds = obj.data_source<impl::ZipFileEntry>();
        if (p_ds == nullptr) {
            auto p_entry = mp_zip->CreateEntry(rel_fpath.string());
            if (looks_like_directory(r_reg, item_fpath, obj)) {
                p_entry->SetAttributes(ZipArchiveEntry::Attributes::Directory);
            }

            p_ds = new impl::ZipFileEntry(p_entry, Origin::MEMORY);
            p_ds->set_options(options());
            p_ds->bind(obj);
        }

        if (p_ds->mp_entry->IsDirectory()) {
            for (auto& item: obj.iter_items())
                fifo.push(std::make_pair(OPath{rel_path, item.first}, item.second));
        } else {
            if (r_reg->has_association(item_fpath)) {
                auto r_serial = r_reg->get_serializer(item_fpath);
                std::stringstream* p_stream = new std::stringstream();
                streams.push_back(p_stream);
                r_serial->write(*p_stream, obj, save_options);
                p_ds->mp_entry->SetCompressionStream(*p_stream);
            }
        }
    }

    // save archive
    std::ofstream f_out{fpath, std::ios::out};

    mp_zip->WriteToStream(f_out);

    if (f_out.bad()) report_write_error(fpath, strerror(errno));
    if (f_out.fail()) report_write_error(fpath, "ostream::fail()");
}

inline
void ZipFile::free_resources() {
    if (mp_zip) mp_zip.reset();
}

inline
void impl::ZipFileEntry::read(const Object& target) {
    auto r_reg = get_registry(target);
    auto fpath = path(target);
    auto r_ser = r_reg->get_serializer(fpath);
    ASSERT(r_ser);

    auto p_stream = mp_entry->GetDecompressionStream();
    Finally finally{ [this] () { this->mp_entry->CloseDecompressionStream(); } };

    if (p_stream) {
        auto& stream = *p_stream;
        Object obj = r_ser->read(stream, mp_entry->GetSize());
        if (!obj.is_valid()) {
            report_read_error(fpath, obj.to_str());
        } else if (stream.bad()) {
            report_read_error(fpath, strerror(errno));
        } else if (!stream.eof() && stream.fail()) {
            report_read_error(fpath, "ostream::fail");
        } else {
            read_set(target, obj);
        }
    }
}

} // namespace filesystem
} // namespace nodel

