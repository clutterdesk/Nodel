#include <gtest/gtest.h>
#include <cpptrace/cpptrace.hpp>
#include <nodel/support/logging.h>
#include <nodel/support/intern.h>
#include <stdlib.h>
#include <string_view>

NODEL_THREAD_LOCAL_INTERNS;

int main(int argc, char **argv) {
  cpptrace::register_terminate_handler();

  // build new arg list
  std::vector<char*> args;
  for (int i=0; i<argc; i++)
      args.push_back(argv[i]);

  // add --gtest_catch_exceptions=0
  args.push_back((new std::string("--gtest_catch_exceptions=0"))->data());

  argc = args.size();
  argv = args.data();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
