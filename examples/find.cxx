/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <nodel/core.hxx>
#include <nodel/filesystem.hxx>
#include <regex>

NODEL_INIT_CORE;
NODEL_INIT_FILESYSTEM;

using namespace nodel;

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "usage: find <path> <regex>" << std::endl;
        return 1;
    }

    filesystem::configure();

    URI uri{"file://?perm=r&path=."};
    uri["path"_key] = argv[1];
    Object dir = bind(uri);

    // visit only those objects that match the regular expression
    auto visit_pred = filesystem::make_regex_filter(argv[2]);

    // only descend into objects that represent directories - not file content
    for (const auto& f: dir.iter_tree_if(visit_pred, filesystem::is_dir)) {
        std::cout << filesystem::path(f).string() << std::endl;
    }
}
