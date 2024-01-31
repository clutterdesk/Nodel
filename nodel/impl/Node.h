#pragma once

#include "Object.h"

#include <regex>


class Node
{
  public:
    Node() {}
    Node(auto v) : child{v} {}
    Node(const std::string& str) : child{str} {}
    Node(std::string&& str) : child{std::move(str)} {}
    Node(const char* str) : child{str} {}

	Node(const Node& other) : Object{other}, parent{other.parent} {}
	Node(Node&& other) : Object{other}, parent{other.parent} {}
	Node& operator = (const Node& other) { parent = other.parent; Object::operator=(other); return *this; }
	Node& operator = (Node&& other) { parent = other.parent; Object::operator=(other); return *this; }

	std::optional<const Node> parent() const { return parent; }
	std::optional<Node> parent() { return parent; }

    template <typename Arg>
    Node operator [] (Arg arg) {
    	Node& self = *this;
    	Node node{child[arg]};
    	node.parent = *this;
    	return node;
    }

    // TODO: handle constness here and in Object
//    template <typename Arg>
//    const Node operator [] (Arg arg) const {
//    	Node node{child[arg]};
//    	node.parent = *this;
//    	return node;
//    }

    template <typename Arg, typename ... Args>
    Node operator [] (Arg arg, Args ... args) {
    	Node node{child[arg, args ...]};
    	node.parent = *this;
    	return node;
    }

    // TODO: handle constness here and in Object
//    template <typename Arg, typename ... Args>
//    Node operator [] (Arg arg, Args ... args) const {
//    	Node node{child[arg, args ...]};
//    	node.parent = *this;
//    	return node;
//    }

  private:
    std::optional<Node> parent;
    Object child;
};

Node::Node(const Node& parent, const Object& object)
  : p_parent{&parent}, object{object}, rcnt{1}
{
	parent.object.inc_ref_count();
}

Node::~Node() {
	if (p_parent != nullptr && p_parent->object.dec_ref_count() == 0
		if (p_parent->object.ref_count
		if (p_p
	}
}
