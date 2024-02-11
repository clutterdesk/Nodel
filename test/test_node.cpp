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

  EXPECT_EQ(root.id(), a.parent().id());
  EXPECT_EQ(root.id(), b.parent().id());

  EXPECT_EQ(a.id(), a[0].parent().id());
  EXPECT_EQ(root.id(), a[0].parent().parent().id());

  EXPECT_EQ(a.id(), a[1].parent().id());
  EXPECT_EQ(root.id(), a[1].parent().parent().id());

  EXPECT_EQ(b.id(), b[0].parent().id());
  EXPECT_EQ(root.id(), b[0].parent().parent().id());

  EXPECT_EQ(b.id(), b[1].parent().id());
  EXPECT_EQ(root.id(), b[1].parent().parent().id());
}

TEST(Node, FindNodeKey) {
    Node root = Node::from_json("{'a': [1, [1], 1], 'b': {'x': 3, 'y': 4}}");
    EXPECT_EQ(root.key_of(root["b"]), "b");
    EXPECT_EQ(root["b"].key(), "b");
    EXPECT_EQ(root["a"][1].key(), 1);

    // NOTE: this doesn't work, but isn't explicitly disallowed for performance reasons
    EXPECT_NE(root["a"][2].key(), 2);
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
    EXPECT_EQ(lhs.id(), lhs["x"].parent().id());
    lhs["x"] = rhs;
    EXPECT_EQ(lhs["x"]["u"], 7);
    EXPECT_EQ(lhs["x"].parent().id(), lhs.id());
}

TEST(Node, AccessAssignMapRefIntegrity) {
    Node rhs = Node::from_json(R"({'u': 7})");
    Node lhs = Node::from_json(R"({'x': {'u': 8}})");
    EXPECT_EQ(lhs.id(), lhs["x"].parent().id());

    Node x = lhs["x"];
    lhs["x"] = rhs;
    EXPECT_EQ(x.parent().id(), lhs.id());
    EXPECT_EQ(x["u"], 8);
    EXPECT_EQ(lhs["x"]["u"], 7);
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

struct TestStore : public IDataStore
{
    TestStore(const char* json, int* deleted=nullptr) : IDataStore{OBJECT}, json{json}, deleted{deleted} {
        if (deleted) *deleted = 0;
    }

    ~TestStore() override { if (deleted) *deleted = 1; }

    Object read(Node& from_node) override {
        return Node::from_json(json);
    }

    Object read(Node& from_node, const Key& key) override {
        Node node = Node::from_json(json);
        return node.get_immed(key);
    }

    void write(Node& to_node, const Node& from_node) override {}
    void write(Node& to_node, const Key& key, const Node& from_node) override {}

    void reset(Node& node) override {}
    void refresh(Node& node) override {}

    const char* json;
    int* deleted;
};

TEST(Node, DataStoreWithInt) {
    Node obj;
    obj.bind<TestStore>("{'a': 1, 'b': 2}");
    EXPECT_EQ(obj.ref_count(), 1);
    EXPECT_EQ(obj["b"], 2);
    EXPECT_EQ(obj["a"], 1);
    EXPECT_TRUE(obj.is_map());
}

//TEST(Node, DataStoreWithMap) {
//    Object obj = Object::new_remote<TestStore>("7");
//    EXPECT_EQ(obj.ref_count(), 1);
//    EXPECT_TRUE(obj == 7);
//    EXPECT_TRUE(obj < 8);
//    EXPECT_TRUE(obj > 6);
//    EXPECT_EQ(obj.as_int(), 7);
//    EXPECT_TRUE(obj.is_int());
//    EXPECT_TRUE(obj.to_bool());
//    EXPECT_EQ(obj.to_fp(), 7.0);
//}
//
//TEST(Node, DataStoreToKey) {
//    Object obj = Object::new_remote<TestStore>("'foo'");
//    Key key = obj.to_key();
//    EXPECT_TRUE(key.is_str());
//    EXPECT_EQ(key, "foo");
//}
//
//TEST(Node, DataStoreToStr) {
//    Object obj = Object::new_remote<TestStore>("'foo'");
//    EXPECT_EQ(obj.to_str(), "foo");
//}
//
//TEST(Node, DataStoreToJson) {
//    Object obj = Object::new_remote<TestStore>("'foo'");
//    EXPECT_EQ(obj.to_json(), R"("foo")");
//}
//
//TEST(Node, FreeDataStore) {
//    int deleted = 0;
//    Object obj = Object::new_remote<TestStore>("'foo'", &deleted);
//    EXPECT_EQ(deleted, 0);
//    obj = 0;
//    EXPECT_EQ(deleted, 1);
//}
//
//TEST(Node, DataStoreWalkDF) {
//  Object obj = Object::new_remote<TestStore>("[1, [2, [3, [4, 5], 6], 7], 8]");
//  std::vector<Int> expect_order{1, 2, 3, 4, 5, 6, 7, 8};
//  std::vector<Int> actual_order;
//
//  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object, int) -> void {
//    if (!object.is_container())
//      actual_order.push_back(object.to_int());
//  };
//
//  WalkDF walk{obj, visitor};
//  while (walk.next()) {}
//
//  EXPECT_EQ(actual_order.size(), expect_order.size());
//  EXPECT_EQ(actual_order, expect_order);
//}
//
//TEST(Node, DataStoreWalkBF) {
//  Object obj = Object::new_remote<TestStore>("[1, [2, [3, [4, 5], 6], 7], 8]");
//  std::vector<Int> expect_order{1, 8, 2, 7, 3, 6, 4, 5};
//  std::vector<Int> actual_order;
//
//  auto visitor = [&actual_order] (const Object& parent, const Key& key, const Object& object) -> void {
//    if (!object.is_container())
//      actual_order.push_back(object.to_int());
//  };
//
//  WalkBF walk{obj, visitor};
//  while (walk.next()) {}
//
//  EXPECT_EQ(actual_order.size(), expect_order.size());
//  EXPECT_EQ(actual_order, expect_order);
//}
