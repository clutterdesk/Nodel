#include <gtest/gtest.h>

#include <nodel/impl/logging.h>

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
