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

class Node
{
  public:
    class Access;

    Node() {}
    Node(const Access&);
    Node(Access&&);
    Node(Object& obj) : child{obj} {}

    Node(const Node& other) : child{other.child}, r_parent{other.r_parent}  {}
    Node(Node&& other) : child{std::move(other.child)}, r_parent{other.r_parent} {}

    Node& operator = (const Node& other) {
//        fmt::print("Node::operator=(const Node&) <- {}\n", other.to_str());
        child = other.child;
        r_parent = other.r_parent;
        return *this;
    }

    Node& operator = (Node&& other) {
//        fmt::print("Node::operator=(Node&&) <- {}\n", other.to_str());
        child = std::move(other.child);
        r_parent = std::move(other.r_parent);
        return *this;
    }

    Node(auto v) : child{v} {}

    const Node parent() const { return (r_parent.get_ptr() != nullptr)? *r_parent: Node{}; }
    Node parent()             { return (r_parent.get_ptr() != nullptr)? *r_parent: Node{}; }

    const Key key() const;
    const Key key_of(const Node& child) const;

    static Node from_json(const std::string& json, std::string& error);
    static Node from_json(const std::string& json);

    template <class LoaderClass, typename ... Args>
    void bind(Args&& ... args) {
        child.free();
        child.dat = new LoaderClass(std::forward<Args>(args) ...);
    }

    bool is_null() const      { lazy(); return child.is_type<void*>(); }
    bool is_bool() const      { lazy(); return child.is_type<bool>(); }
    bool is_int() const       { lazy(); return child.is_type<Int>(); };
    bool is_uint() const      { lazy(); return child.is_type<UInt>(); };
    bool is_float() const     { lazy(); return child.is_type<Float>(); };
    bool is_str() const       { lazy(); return child.is_type<StringPtr>(); };
    bool is_num() const       { lazy(); return child.is_int() || is_uint() || is_float(); }
    bool is_list() const      { lazy(); return child.is_type<ListPtr>(); }
    bool is_map() const       { lazy(); return child.is_type<MapPtr>(); }
    bool is_container() const { lazy(); return child.is_list() || child.is_map(); }

    Int& as_int()         { lazy(); return child.as_int(); }
    UInt& as_uint()       { lazy(); return child.as_uint(); }
    double& as_fp()       { lazy(); return child.as_fp(); }
    std::string& as_str() { lazy(); return child.as_str(); }

    Int as_int() const                { lazy(); return child.as_int(); }
    UInt as_uint() const              { lazy(); return child.as_uint(); }
    double as_fp() const              { lazy(); return child.as_fp(); }
    std::string const& as_str() const { lazy(); return child.as_str(); }

    bool to_bool() const        { lazy(); return child.to_bool(); }
    Int to_int() const          { lazy(); return child.to_int(); }
    UInt to_uint() const        { lazy(); return child.to_uint(); }
    Float to_fp() const         { lazy(); return child.to_fp(); }
    std::string to_str() const  { lazy(); return child.to_str(); }
    Key to_key() const          { lazy(); return child.to_key(); }
    std::string to_json() const { lazy(); return child.to_json(); }

    template <typename Arg>
    Access operator [] (Arg&& arg);

    template <typename Arg>
    Access get(Arg&& arg);

    template <typename Arg, typename ... Args>
    Access get(Arg&& arg, Args&& ... args);

    bool operator == (const Node& other) const  { lazy(); other.lazy(); return child == other.child; }
    auto operator <=> (const Node& other) const { lazy(); other.lazy(); return child <=> other.child; }

    Oid id() const { lazy(); return child.id(); }
    size_t hash() const { lazy(); return child.hash(); }
    size_t ref_count() const { return child.ref_count(); }

  protected:
    Node(Node* p_parent, Object&& child) : child{std::move(child)}, r_parent{p_parent} {}

    void lazy() const;

  private:
    Object child;
    impl::NodeRef r_parent;

  friend class impl::NodeRef;
  friend class Node::Access;
};


class Node::Access : public Node
{
  public:
    Access& operator = (const Node& node);
    Access& operator = (Node&& node);

  protected:
    Access(Node* p_parent, Object&& child) : Node{p_parent, std::forward<Object>(child)} {}
    Access(const Access& other) : Node{other} {}
    Access(Access&& other) : Node{std::forward<Access>(other)} {}

  friend class Node;
};


inline
Node::Node(const Node::Access& access) : Node{(Node&)access} {}

inline
Node::Node(Node::Access&& access) : Node{std::forward<Node>((Node&&)access)} {}

template <typename Arg>
Node::Access Node::operator [] (Arg&& arg) {
    return get(std::forward<Arg>(arg));
}

template <typename Arg>
Node::Access Node::get(Arg&& arg) {
    lazy();
    return {this, child.get(std::forward<Arg>(arg))};
}

template <typename Arg, typename ... Args>
Node::Access Node::get(Arg&& arg, Args&& ... args) {
    Node node{get(arg)};
    return node.get(std::forward<Args>(args) ...);
}

inline
Node::Access& Node::Access::operator = (const Node& node) {
//    fmt::print("Access::operator=(const Node&) <- {}\n", node.to_str());
    lazy();
    node.lazy();
    if (r_parent.is_null()) {
        child = node.child;
    } else {
        std::visit(overloaded {
            [] (auto&&) {},
            [this, &node] (ListPtr ptr) { std::get<0>(*ptr).at(key().to_uint()) = node.child; },
            [this, &node] (MapPtr ptr) { std::get<0>(*ptr).at(key()) = node.child; }
        }, r_parent->child.dat);
    }
    return *this;
}

inline
Node::Access& Node::Access::operator = (Node&& node) {
//    fmt::print("Access::operator=(Node&&) <- {}\n", node.to_str());
    lazy();
    node.lazy();
    if (r_parent.is_null()) {
        child = std::move(node.child);
    } else {
        std::visit(overloaded {
            [] (auto&&) {},
            [this, &node] (ListPtr ptr) { std::get<0>(*ptr).at(key().to_uint()) = std::move(node.child); },
            [this, &node] (MapPtr ptr) { std::get<0>(*ptr).at(key()) = std::move(node.child); }
        }, r_parent->child.dat);
    }
    return *this;
}

inline
const Key Node::key_of(const Node& node) const {
    lazy();
    node.lazy();

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

inline
void Node::lazy() const {
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
