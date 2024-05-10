/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <nodel/core.h>
#include <nodel/rocksdb.h>
#include <nodel/support/Stopwatch.h>

NODEL_INIT_CORE;
NODEL_INIT_FILESYSTEM;

using namespace nodel;

int main(int argc, char** argv) {
    nodel::filesystem::configure();
    nodel::rocksdb::configure();

    std::filesystem::remove_all("nodel_example.rocksdb");

    debug::Stopwatch swatch;
    swatch.start("Profile Creating Data Without DB Overhead");
    Object db = bind("rocksdb://?perm=rw&path=nodel_example.rocksdb");
    db.reserve((size_t)1e7);
    for (size_t i=0; i<1e7; i++)
        db.set(i, i);
    auto elapsed_without_save = swatch.finish();

    swatch.start("Profile Creating Data and Saving to DB");
    db = bind("rocksdb://?perm=rw&path=nodel_example.rocksdb");
    for (size_t i=0; i<1e7; i++)
        db.set(i, i);
    db.save();
    auto elapsed_with_save = swatch.finish();

    std::cout << "Nodel Overhead=" << (100.0 * elapsed_without_save / elapsed_with_save) << "%"<< std::endl;

    swatch.start("Count keys");
    db.reset();
    auto count = algo::count(db.iter_keys());
    swatch.finish();
    std::cout << "Total number of entries=" << count << std::endl;

    swatch.start("Count keys in [5000700, 5000705)");
    db.reset();
    count = algo::count(db.iter_keys({{5000700UL}, {5000705UL}}));
    swatch.finish();
    std::cout << "Number of entries in slice=" << count << std::endl;
}
