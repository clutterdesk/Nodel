/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

inline
void SerialFile::read(const Object& target) {
    auto fpath = path(target).string();
    auto size = std::filesystem::file_size(fpath);
    std::ifstream f_in{fpath, std::ios::in | std::ios::binary};

    Object obj = mr_serial->read(f_in, size);
    if (!obj.is_valid()) {
        report_read_error(fpath, obj.to_str());
    } else if (f_in.bad()) {
        report_read_error(fpath, strerror(errno));
    } else if (!f_in.eof() && f_in.fail()) {
        report_read_error(fpath, "ostream::fail");
    } else {
        read_set(target, obj);
    }
}

inline
void SerialFile::write(const Object& target, const Object& cache, const Object& options) {
    auto fpath = path(target).string();
    std::ofstream f_out{fpath, std::ios::out};

    mr_serial->write(f_out, cache, options);

    if (f_out.bad()) report_write_error(fpath, strerror(errno));
    if (f_out.fail()) report_write_error(fpath, "ostream::fail()");
}
