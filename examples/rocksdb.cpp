#include <nodel/core.h>
#include <nodel/rocksdb.h>
#include <nodel/support/Stopwatch.h>

NODEL_INIT_CORE;
NODEL_INIT_FILESYSTEM;

using namespace nodel;

int main(int argc, char** argv) {
    nodel::filesystem::configure();
    nodel::rocksdb::configure();

    std::cout << "Create DB ..." << std::endl;
    debug::Stopwatch swatch;
    swatch.start();
    Object db = bind("rocksdb://?perm=rw&path=example.rocksdb");
    size_t k=0;
    for (size_t i=0; i<10; i++) {
        for (size_t j=0; j<1000000; ++j, ++k) {
            db.set(k, k);
        }
        db.save();
        db.reset();
    }
    db.save();
    swatch.stop();
    swatch.log();

    std::cout << "Count keys ..." << std::endl;
    swatch.clear();
    swatch.start();
    db.reset();
    k=0;
    for (auto key : db.iter_keys()) {
        ++k;
    }
    swatch.stop();
    swatch.log();

    std::cout << "keys=" << k << std::endl;
}
