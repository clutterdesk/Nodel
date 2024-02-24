#pragma once

struct FileSystemDataSource : public IDataSource
{
    FileSystemDataSource() {
        data = parse_json(json);
    }

    ~FileSystemDataSource() override {}

    Object read() override {
        read_called = false;
        read_key_called = true;
        if (cached.is_empty()) cached = data;
        return cached;
    }

    Object read_key(const Key& key) override {
        read_called = false;
        read_key_called = true;
        if (cached.is_empty()) cached = data;
        return cached[key];
    }

    void write(const Object& obj) override {
        memory_bypass = false;
        if (obj.has_data_source()) {  // for test purposes, assume same data-source
            // cache bypass
            memory_bypass = true;
            auto& dsrc = *dynamic_cast<ShallowSource*>(obj.data_source());
            data.empty();
            data = dsrc.data;
        } else {
            data.empty();
            data = obj;
        }
    }

    void write(Object&& obj) override {
        memory_bypass = false;
        if (obj.has_data_source()) {  // for test purposes, assume same data-source
            // cache bypass
            memory_bypass = true;
            auto& dsrc = *dynamic_cast<ShallowSource*>(obj.data_source());
            data.empty();
            data = dsrc.data;
        } else {
            data.empty();
            data = std::forward<Object>(obj);
        }
    }

    struct Iterator : public IDataSourceIterator
    {
        size_t chunk_size = 3;

        Iterator(Object data) : data{data} {}

        size_t iter_begin() override { return chunk_size; }

        size_t iter_next(std::string& chunk) override {
            std::string& str = data.as<String>();
            chunk.resize(std::min(chunk_size, str.size() - pos));
            memcpy(chunk.data(), data.as<String>().data() + pos, chunk.size());
            pos += chunk.size();
            return chunk.size();
        }

        size_t iter_next(List& chunk) override {
            auto values = data.values();
            chunk.resize(std::min(chunk.size(), values.size() - pos));
            for (size_t i=0; i < chunk.size(); i++) {
                chunk[i] = values[pos + i];
            }
            pos += chunk.size();
            return chunk.size();
        }

        size_t iter_next(KeyList& chunk) override {
            auto keys = data.keys();
            chunk.resize(std::min(chunk.size(), keys.size() - pos));
            for (size_t i=0; i < chunk.size(); i++) {
                chunk[i] = keys[pos + i];
            }
            pos += chunk.size();
            return chunk.size();
        }

        void iter_end() override { delete this; }

        Object data;
        int pos = 0;
    };

    IDataSourceIterator* iter() override {
        return new Iterator{data};
    }

    size_t size() override {
        if (cached.is_empty()) cached = data;
        return cached.size();
    }

    Object::ReprType type() const override {
        if (cached_type == Object::BAD_I) {
            std::istringstream in{data.to_json()};
            json::impl::Parser parser{in};
            const_cast<ShallowSource*>(this)->cached_type = parser.parse_type();
        }
        return cached_type;
    }

    Oid id() const override {
        return data.id();
    }

    void reset() override   { cached.empty(); }
    void refresh() override {}  // not implemented

    Object data;
    Object::ReprType cached_type = Object::BAD_I;
    Object cached;
    bool read_called = false;
    bool read_key_called = false;
    bool memory_bypass = false;
};
