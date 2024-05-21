/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <gtest/gtest.h>
#include <fmt/core.h>
#include <nodel/core/uri.hxx>

using namespace nodel;

TEST(URI, Basic) {
    Object obj = URI("http://user@host:1234");
    ASSERT_EQ(obj.size(), 4UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("host"_key), "host");
    EXPECT_EQ(obj.get("port"_key), 1234);
}

TEST(URI, OnlyPath) {
    Object obj = URI("http:///");
    ASSERT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("path"_key), "/");
}

TEST(URI, NoUser) {
    Object obj = URI("http://host.com");
    ASSERT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("host"_key), "host.com");

    obj = URI("http://host.com:1234");
    ASSERT_EQ(obj.size(), 3UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("host"_key), "host.com");
    EXPECT_EQ(obj.get("port"_key), 1234);
}

TEST(URI, NoUserAndQuery) {
    Object obj = URI("http://host.com?k1=v1");
    ASSERT_EQ(obj.size(), 3UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("host"_key), "host.com");
    EXPECT_EQ(obj.get("query.k1"_path), "v1");

    obj = URI("http://host.com:1234?k1=v1");
    ASSERT_EQ(obj.size(), 4UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("host"_key), "host.com");
    EXPECT_EQ(obj.get("port"_key), 1234);
    EXPECT_EQ(obj.get("query.k1"_path), "v1");
}

TEST(URI, NoHost) {
    Object obj = URI("http://user@");
    ASSERT_EQ(obj.size(), 2UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");

    obj = URI("http://user@:1234");
    ASSERT_EQ(obj.size(), 3UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("port"_key), 1234);
}

TEST(URI, NoHostAndQuery) {
    Object obj = URI("http://user@?k1=v1");
    ASSERT_EQ(obj.size(), 3UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("query.k1"_path), "v1");

    obj = URI("http://user@:1234?k1=v1");
    ASSERT_EQ(obj.size(), 4UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("port"_key), 1234);
    EXPECT_EQ(obj.get("query.k1"_path), "v1");
}

TEST(URI, Path) {
    Object obj = URI("http://user@host:1234/a/b/c");
    ASSERT_EQ(obj.size(), 5UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("host"_key), "host");
    EXPECT_EQ(obj.get("port"_key), 1234);
    EXPECT_EQ(obj.get("path"_key), "/a/b/c");
}

TEST(URI, Query) {
    Object obj = URI("http://user@host:1234?k1=v1&k2=v2");
    ASSERT_EQ(obj.size(), 5UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("host"_key), "host");
    EXPECT_EQ(obj.get("port"_key), 1234);
    EXPECT_EQ(obj.get("query.k1"_path), "v1");
    EXPECT_EQ(obj.get("query.k2"_path), "v2");
}

TEST(URI, PathAndQuery) {
    Object obj = URI("http://user@host:1234/a/b/c?k1=v1");
    ASSERT_EQ(obj.size(), 6UL);
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("host"_key), "host");
    EXPECT_EQ(obj.get("port"_key), 1234);
    EXPECT_EQ(obj.get("path"_key), "/a/b/c");
    EXPECT_EQ(obj.get("query.k1"_path), "v1");
 }

TEST(URI, EmptyQuery) {
    Object obj = URI("http://user@host:1234?");
    EXPECT_TRUE(obj == nil);
}

TEST(URI, EmptyPath) {
    Object obj = URI("http://user@host:1234/");
    EXPECT_EQ(obj.get("scheme"_key), "http");
    EXPECT_EQ(obj.get("user"_key), "user");
    EXPECT_EQ(obj.get("host"_key), "host");
    EXPECT_EQ(obj.get("port"_key), 1234);
    EXPECT_EQ(obj.get("path"_key), "/");
}

