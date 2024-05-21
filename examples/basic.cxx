/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <nodel/core.hxx>

NODEL_INIT_CORE;

using namespace nodel;

int main(int argc, char** argv) {
    // create ordered-map from json
    auto obj = "{'text': 'string', 'pi': 3.1415926, 'array': [1, 2]}"_json;

    // access with key
    std::cout << "text="     << obj.get("text"_key)     << std::endl;
    std::cout << "pi="       << obj["pi"_key]           << std::endl;
    std::cout << "array[0]=" << obj["array"_key][0] << std::endl;

    // access with path
    std::cout << "array[1]=" << obj.get("array[1]"_path) << std::endl;

    // update with key
    obj.set("text"_key, "new string");
    obj["pi"_key] = 3.141592654;

    // update with path
    obj["array[0]"_path] = "1";  // change 1 to "1"

    // print
    std::cout << "obj after updates=" << obj << std::endl;

    // delete with key
    obj.del("text"_key);

    // delete with path
    obj.del("array[0]"_path);

    // print
    std::cout << "obj after deletes=" << obj << std::endl;
}
