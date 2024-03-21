#include <gtest/gtest.h>

#include <nodel/support/logging.h>
#include <nodel/support/intern.h>

NODEL_INTERN_STATIC_INIT;

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
