#include <gtest/gtest.h>
#include <nodel/nodel.h>
#include <fmt/core.h>

using namespace nodel;

TEST(Node, String) {
  Node node{"food"};
  EXPECT_TRUE(node.is_str());
  EXPECT_EQ(node.to_json(), "\"food\"");
}

TEST(Node, FromJson) {
  Node node = Node::from_json("'food'");
  EXPECT_TRUE(node.is_str());
  EXPECT_EQ(node.to_json(), "\"food\"");
}

TEST(Node, FindNodeKey) {
    Node root = Node::from_json("{'a': [1, [1], 1], 'b': {'x': 3, 'y': 4}}");
    EXPECT_EQ(root.key_of(root["b"]), "b");
    EXPECT_EQ(root["b"].key(), "b");
    EXPECT_EQ(root["a"][1].key(), 1);

    // NOTE: this doesn't work, but isn't explicitly disallowed for performance reasons
    EXPECT_NE(root["a"][2].key(), 2);
}

TEST(Node, SubscriptCheckParent) {
  Node node = Node::from_json("['a', 'b']");
  EXPECT_TRUE(node.is_list());
  EXPECT_EQ(node.id(), node[0].parent().id());
  EXPECT_EQ(node.id(), node[1].parent().id());
  EXPECT_EQ(node.ref_count(), 1);
}

TEST(Node, MultipleSubscriptCheckAncestors) {
  Node root = Node::from_json("{'a': [1, 2], 'b': [3, 4]}");
  EXPECT_TRUE(root.is_map());

  Node a = root["a"];
  Node b = root["b"];

  EXPECT_EQ(a.id(), a[0].parent().id());
  EXPECT_EQ(root.id(), a[0].parent().parent().id());

  EXPECT_EQ(a.id(), a[1].parent().id());
  EXPECT_EQ(root.id(), a[1].parent().parent().id());

  EXPECT_EQ(b.id(), b[0].parent().id());
  EXPECT_EQ(root.id(), b[0].parent().parent().id());

  EXPECT_EQ(b.id(), b[1].parent().id());
  EXPECT_EQ(root.id(), b[1].parent().parent().id());
}

TEST(Node, AssignList) {
    Node rhs = Node::from_json(R"([7, 8])");
    Node lhs;
    lhs = rhs;
    EXPECT_EQ(lhs[0], 7);
    EXPECT_EQ(lhs[1], 8);
    EXPECT_EQ(lhs[0].parent().id(), lhs.id());
    EXPECT_TRUE(rhs.parent().is_null());
}

TEST(Node, AssignMap) {
    Node rhs = Node::from_json(R"({'x': 7})");
    Node lhs;
    lhs = rhs;
    EXPECT_EQ(lhs["x"], 7);
    EXPECT_EQ(lhs["x"].parent().id(), lhs.id());
    EXPECT_TRUE(rhs.parent().is_null());
}

TEST(Node, AccessAssignMap) {
    Node rhs = Node::from_json(R"({'u': 7})");
    Node lhs = Node::from_json(R"({'x': {}})");
    lhs["x"] = rhs;
    EXPECT_EQ(lhs["x"], 7);
    EXPECT_EQ(lhs["x"].parent().id(), lhs.id());
    EXPECT_TRUE(rhs.parent().is_null());
}

TEST(Node, RedundantAssignment) {
  Node root = Node::from_json("{'a': [1, 2], 'b': [3, 4]}");
  EXPECT_TRUE(root.is_map());

  Node other = root;
  root = other;
  other = Node{};
  EXPECT_TRUE(root.is_map());
  EXPECT_EQ(root.ref_count(), 1);
}

TEST(Node, RefCountIntegrity) {
  Object foo{"foo"};
  Node* p_node = new Node{foo};
  EXPECT_EQ(foo.ref_count(), 2);
  EXPECT_EQ(p_node->ref_count(), foo.ref_count());

  Node* p_copy = new Node(*p_node);
  EXPECT_EQ(foo.ref_count(), 3);
  EXPECT_EQ(p_node->ref_count(), foo.ref_count());
  EXPECT_EQ(p_copy->ref_count(), foo.ref_count());

  delete p_node;
  EXPECT_EQ(foo.ref_count(), 2);
  EXPECT_EQ(p_copy->ref_count(), foo.ref_count());

  delete p_copy;
  EXPECT_EQ(foo.ref_count(), 1);
}

TEST(Node, ParentRefCountIntegrity) {
  Node root = Node::from_json("{'a': [[1], [2]], 'b': [[3], [4]]}");
  EXPECT_TRUE(root.is_map());

  Node a = root["a"];
  Node a0 = a[0];
  Node a1 = a[1];

  Node b = root["b"];

  EXPECT_EQ(root.ref_count(), 3);
  EXPECT_EQ(a.ref_count(), 4);

  a0 = Node{};
  EXPECT_EQ(root.ref_count(), 3);
  EXPECT_EQ(a.ref_count(), 3);

  a = Node{};
  EXPECT_EQ(root.ref_count(), 2);
  EXPECT_EQ(a1.ref_count(), 2);
  EXPECT_EQ(a1[0], 2);

  b = Node{};
  EXPECT_EQ(root.ref_count(), 1);
}

TEST(Node, AssignmentScopes) {
    Node node = Node::from_json("{'x': {'a': 1, 'b': 2}}");
    node["x"] = Node::from_json("{'u': 3, 'v': 4}");
    auto val = node.get("x").get("u");
    fmt::print("{}\n", val.to_str());
    EXPECT_EQ(val, 3);
}
