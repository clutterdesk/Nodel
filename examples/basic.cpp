#include <nodel/core.h>

NODEL_INIT_CORE;

using namespace nodel;

int main(int argc, char** argv) {
    // create ordered-map from json
    auto obj = "{'text': 'string', 'pi': 3.1415926, 'array': [1, 2]}"_json;

    // read single key
    std::cout << "text=" << obj.get("text"_key) << std::endl;
    std::cout << "pi=" << obj.get("pi"_key) << std::endl;

    // read path
    std::cout << "array[1]=" << obj.get("array[1]"_path) << std::endl;
}
