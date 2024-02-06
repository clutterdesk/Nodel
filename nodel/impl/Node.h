#pragma once

#include "Object.h"

#include <fmt/core.h>

namespace nodel {

class Node;

namespace impl {

class NodeRef
{
  public:
    NodeRef() {}
    NodeRef(Node* p_node) { set(p_node); }

    ~NodeRef() { reset(); }

    NodeRef(const NodeRef& other) { set(other.ptr); }
    NodeRef(NodeRef&& other){ set(other.ptr); }
    NodeRef& operator = (const NodeRef& other) { set(other.ptr); return *this; }
    NodeRef& operator = (NodeRef&& other) { set(other.ptr); return *this; }

    void set(const Node*);
    void reset();

    bool is_null() const { return ptr == nullptr; }

    const Node& operator * () const { return *ptr; }
    Node& operator * () { return *ptr; }

    const Node* operator -> () const { return ptr; }
    Node* operator -> () { return ptr; }

    const Node* get_ptr() const { return ptr; }
    Node* get_ptr() { return ptr; }

  private:
    Node* ptr = nullptr;
};

} // namespace impl

class NodeAccess;

class Node
{
  public:
    Node() {}
    Node(const NodeAccess&);
    Node(NodeAccess&&);
    Node(Object& obj) : child{obj} {}

    Node(const Node& other) : child{other.child}, r_parent{other.r_parent}  {}
    Node(Node&& other) : child{std::move(other.child)}, r_parent{other.r_parent} {}

    Node& operator = (const Node& other) {
        child = other.child;
        return *this;
    }

    Node& operator = (Node&& other) {
        child = std::move(other.child);
        return *this;
    }

    Node(auto v) : child{v} {}

    const Node parent() const { return (r_parent.get_ptr() != nullptr)? *r_parent: Node{}; }
    Node parent() { return (r_parent.get_ptr() != nullptr)? *r_parent: Node{}; }

    const Key key() const;
    const Key key_of(const Node& child) const;

    static Node from_json(const std::string& json, std::string& error);
    static Node from_json(const std::string& json);

    template <class LoaderClass, typename ... Args>
    void bind(Args&& ... args) {
        child.free();
        child.dat = new LoaderClass(std::forward<Args>(args) ...);
    }

    bool is_null() const      { return child.is_type<void*>(); }
    bool is_bool() const      { return child.is_type<bool>(); }
    bool is_int() const       { return child.is_type<Int>(); };
    bool is_uint() const      { return child.is_type<UInt>(); };
    bool is_float() const     { return child.is_type<Float>(); };
    bool is_str() const       { return child.is_type<StringPtr>(); };
    bool is_num() const       { return child.is_int() || is_uint() || is_float(); }
    bool is_list() const      { return child.is_type<ListPtr>(); }
    bool is_map() const       { return child.is_type<MapPtr>(); }
    bool is_container() const { return child.is_list() || child.is_map(); }

    Int& as_int() { return child.as_int(); }
    Int as_int() const{ return child.as_int(); }
    UInt& as_uint() { return child.as_uint(); }
    UInt as_uint() const{ return child.as_uint(); }
    double& as_fp() { return child.as_fp(); }
    double as_fp() const{ return child.as_fp(); }
    std::string& as_str() { return child.as_str(); }
    std::string const& as_str() const { return child.as_str(); }

    bool to_bool() const { return child.to_bool(); }
    Int to_int() const { return child.to_int(); }
    UInt to_uint() const { return child.to_uint(); }
    Float to_fp() const { return child.to_fp(); }
    std::string to_str() const { return child.to_str(); }
    Key to_key() const { return child.to_key(); }
    std::string to_json() const { return child.to_json(); }

    template <typename Arg>
    NodeAccess operator [] (Arg&& arg);

    template <typename Arg>
    NodeAccess get(Arg&& arg);

    template <typename Arg, typename ... Args>
    NodeAccess get(Arg&& arg, Args&& ... args);

    bool operator == (const Node& other) const { return child == other.child; }
    auto operator <=> (const Node& other) const { return child <=> other.child; }

    Oid id() const { return child.id(); }
    size_t hash() const { return child.hash(); }
    size_t ref_count() const { return child.ref_count(); }

  protected:
    Node(Node* p_parent, Object&& child) : child{std::move(child)}, r_parent{p_parent} {}

  private:
    Object child;
    impl::NodeRef r_parent;

  friend class impl::NodeRef;
  friend class NodeAccess;
};


class NodeAccess : public Node
{
  public:
    NodeAccess& operator = (const Node& node);
    NodeAccess& operator = (Node&& node);

  protected:
    NodeAccess(Node* p_parent, Object&& child) : Node{p_parent, std::forward<Object>(child)} {}

  friend class Node;
};


inline
Node::Node(const NodeAccess& access) : Node{(Node&)access} {}

inline
Node::Node(NodeAccess&& access) : Node{std::forward<Node>((Node&&)access)} {}

template <typename Arg>
NodeAccess Node::operator [] (Arg&& arg) {
    return get(std::forward<Arg>(arg));
}

template <typename Arg>
NodeAccess Node::get(Arg&& arg) {
    return {this, child.get(std::forward<Arg>(arg))};
}

template <typename Arg, typename ... Args>
NodeAccess Node::get(Arg&& arg, Args&& ... args) {
    Node node{get(arg)};
    return node.get(std::forward<Args>(args) ...);
}

inline
NodeAccess& NodeAccess::operator = (const Node& node) {
    std::visit(overloaded {
        [] (auto&&) {},
        [this, &node] (ListPtr ptr) { std::get<0>(*ptr).at(key().to_uint()) = node.child; },
        [this, &node] (MapPtr ptr) { std::get<0>(*ptr).at(key()) = node.child; }
    }, child.dat);
    return *this;
}

inline
NodeAccess& NodeAccess::operator = (Node&& node) {
    child.free();
    node.child.inc_ref_count();
    child.dat.swap(node.child.dat);
    return *this;
}

inline
const Key Node::key_of(const Node& node) const {
    if (child.is_list()) {
        Oid node_id = node.child.id();
        const List& list = child.as_list();
        UInt index = 0;
        for (const auto& item : list) {
            if (item.id() == node_id)
                return Key{index};
            index++;
        }
    } else if (child.is_map()) {
        Oid node_id = node.child.id();
        const Map& map = child.as_map();
        for (auto& [key, value] : map) {
            if (value.id() == node_id)
                return key;
        }
    }
    return Key{};
}

inline
const Key Node::key() const {
    if (r_parent.get_ptr() != nullptr) {
        return r_parent->key_of(child);
    }
    return Key{};
}

namespace impl {

inline
void NodeRef::set(const Node* p_const_node) {
    Node* new_ptr = const_cast<Node*>(p_const_node);
    Node* old_ptr = ptr;
    ptr = new_ptr;
    if (ptr != nullptr) ptr->child.inc_ref_count();
    if (old_ptr != nullptr) old_ptr->child.dec_ref_count();
}

inline
void NodeRef::reset() {
    if (ptr != nullptr) {
        ptr->child.dec_ref_count();
        ptr = nullptr;
    }
}

} // namespace impl

} // namespace nodel
