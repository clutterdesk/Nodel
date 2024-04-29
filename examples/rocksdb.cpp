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
    swatch.start("Create DB");
    Object db = bind("rocksdb://?perm=rw&path=nodel_example.rocksdb");
    size_t k=0;
    for (size_t i=0; i<10; i++) {
        for (size_t j=0; j<1000000; ++j, ++k) {
            db.set(k, k);
        }
        db.save();
        db.reset();  // free memory
    }
    db.save();
    swatch.finish();

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
