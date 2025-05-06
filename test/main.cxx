/// @file
/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>

#include <stdlib.h>
#include <string_view>

#include <nodel/core.hxx>
#include <nodel/filesystem.hxx>
//#include <fmt/core.h>
//#include <nodel/support/leak_detect.hxx>

NODEL_INIT_CORE;
NODEL_INIT_FILESYSTEM;
//NODEL_INIT_LEAK_DETECT;


int main(int argc, char **argv) {
  nodel::filesystem::configure();

  // build new arg list
  std::vector<char*> args;
  for (int i=0; i<argc; i++)
      args.push_back(argv[i]);

  // add --gtest_catch_exceptions=0
  args.push_back((new std::string("--gtest_catch_exceptions=0"))->data());

  argc = args.size();
  argv = args.data();

  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();

//  size_t leaks = 0;
//  for (auto entry : nodel::ref_tracker) {
//      auto p_obj = entry.first;
//      auto count = entry.second;
//      if (count > 0) {
//          if (p_obj->is_empty())
//              fmt::print("LEAK: EMPTY\n");
//          else
//              fmt::print("LEAK: {}\n", p_obj->path().to_str());
//          ++leaks;
//      }
//  }
//  fmt::print("refs={}, leaks={}\n", nodel::ref_tracker.size(), leaks);

  return rc;
}
