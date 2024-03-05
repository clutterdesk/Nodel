#pragma once

#include "Object.h"

#include <type_traits>
#include <fmt/core.h>

namespace nodel {

class Node;

class IDataStore
{
  public:
    enum SyncMode {
        OBJECT,
        KEY
    };

  public:
    IDataStore(SyncMode sync_mode) : sync_mode{sync_mode} {}
    virtual ~IDataStore() {}

    bool is_synced(Node& node);
    bool is_synced(Node& node, const Key& key);

    virtual Object read(Node& from_node) = 0;
    virtual Object read(Node& from_node, const Key& key) = 0;

    // creating a change-log requires being able to associate changes to a specific Node, but
    // if the Node has not yet been loaded, it has null identity.
    virtual void write(Node& to_node, const Node& from_node) = 0;
    virtual void write(Node& to_node, const Key& to_key, const Node& from_node) = 0;

    virtual void reset(Node&) = 0;
    virtual void refresh(Node&) = 0;

    refcnt_t dec_ref_count() { return --ref_count; }
    void inc_ref_count()        { ++ref_count; }

  protected:
    virtual bool is_key_synced(Node& node, const Key& key) {
        throw std::logic_error{"DataStore with sync_mode=KEY does not implement is_key_synced."};
    }

  private:
    SyncMode sync_mode;
    refcnt_t ref_count = 1;
};


namespace impl {

class NodeRef
{
  public:
    NodeRef() {}
    NodeRef(const Node& p_node) { set(p_node); }

    ~NodeRef() { reset(); }

    NodeRef(const NodeRef& other) { set(other.ptr); }
    NodeRef(NodeRef&& other){ set(other.ptr); }
    NodeRef& operator = (const NodeRef& other) { set(other.ptr); return *this; }
    NodeRef& operator = (NodeRef&& other) { set(other.ptr); return *this; }

    void set(const Node&);
    void reset();

    bool is_null() const { return ptr == nullptr; }

    const Node& operator * () const { return *ptr; }
    Node& operator * () { return *ptr; }

    const Node* operator -> () const { return ptr; }
    Node* operator -> () { return ptr; }

    const Node* get_ptr() const { return ptr; }
    Node* get_ptr() { return ptr; }

    operator bool () const { return ptr != nullptr; }

  private:
    Node* ptr = nullptr;
};

class DataStoreRef
{
  public:
    DataStoreRef() = default;

    ~DataStoreRef() {
        if (ptr != nullptr && ptr->dec_ref_count() == 0) {
            delete ptr;
            ptr = nullptr;
        }
    }

    DataStoreRef(const DataStoreRef& other) : ptr{other.ptr}       { if (ptr != nullptr) ptr->inc_ref_count(); }
    DataStoreRef(DataStoreRef&& other) : ptr{std::move(other.ptr)} { other.ptr = nullptr; }

    DataStoreRef& operator = (const DataStoreRef& other) {
        if (ptr != nullptr && ptr->dec_ref_count() == 0) delete ptr;
        ptr = other.ptr;
        if (ptr != nullptr) ptr->inc_ref_count();
        return *this;
    }

    DataStoreRef& operator = (DataStoreRef&& other) {
        if (ptr != nullptr && ptr->dec_ref_count() == 0) delete ptr;
        ptr = std::move(other.ptr);
        other.ptr = nullptr;
        return *this;
    }

    template <class StoreClass, typename ... Args>
    void emplace(Args&& ... args) {
        ptr = new StoreClass(std::forward<Args>(args) ...);
    }

    bool is_synced(Node& node) const;

    template <typename Arg>
    bool is_synced(Node& node, Arg&& arg) const;

    IDataStore* operator -> () const { return ptr; }

    operator bool () const { return ptr != nullptr; }

  private:
    IDataStore* ptr = nullptr;
};

} // namespace impl


template <typename T>
class Access;


class Node
{
  public:
    Node() {}
    Node(Object& obj) : object{obj} {}

    Node(const Access<Key>&);
    Node(Access<Key>&&);
    Node& operator = (const Access<Key>& other);
    Node& operator = (Access<Key>&& other);

    Node(const Node& other) : object{other.object}, r_parent{other.r_parent}  {}
    Node(Node&& other) : object{std::move(other.object)}, r_parent{other.r_parent} {}

    Node& operator = (const Node& other) {
        fmt::print("Node::operator=(const Node&) <- {}\n", other.to_str());
        if (r_store && !r_store.is_synced(*this)) {
            r_store->write(*this, other);
        } else {
            object = other.object;
            r_parent = other.r_parent;
            r_store = other.r_store;
        }
        return *this;
    }

    Node& operator = (Node&& other) {
        fmt::print("Node::operator=(Node&&) <- {}\n", other.to_str());
        if (r_store && !r_store.is_synced(*this)) {
            r_store->write(*this, std::forward<Node>(other));
        } else {
            object = std::move(other.object);
            r_parent = std::move(other.r_parent);
            r_store = std::move(other.r_store);
        }
        return *this;
    }

    Node(auto v) : object{v} {}

    const Node parent() const { return r_parent? *r_parent: Node{}; }
    Node parent()             { return r_parent? *r_parent: Node{}; }

    const Key key() const;
    const Key key_of(const Node& child) const;

    static Node from_json(const std::string& json, std::string& error);
    static Node from_json(const std::string& json);

    template <class StoreClass, typename ... Args>
    void bind(Args&& ... args) {
        r_store.emplace<StoreClass>(std::forward<Args>(args) ...);
    }

    bool has_data_store() const { return r_store; }

    bool is_null() const        { return get_object().is_type<void*>(); }
    bool is_bool() const        { return get_object().is_type<bool>(); }
    bool is_int() const         { return get_object().is_type<Int>(); }
    bool is_uint() const        { return get_object().is_type<UInt>(); }
    bool is_float() const       { return get_object().is_type<Float>(); }
    bool is_str() const         { return get_object().is_type<StringPtr>(); }
    bool is_num() const         { return get_object().is_int() || is_uint() || is_float(); }
    bool is_list() const        { return get_object().is_type<ListPtr>(); }
    bool is_map() const         { return get_object().is_type<MapPtr>(); }
    bool is_container() const   { return get_object().is_list() || object.is_map(); }

    template <typename T> const T& as() const        { return get_object().as<T>(); }
    template <typename T> T& as()                    { return get_object().as<T>(); }
    template <typename T> const T& unsafe_as() const { return get_object().unsafe_as<T>(); }
    template <typename T> T& unsafe_as()             { return get_object().unsafe_as<T>(); }

    bool to_bool() const        { return get_object().to_bool(); }
    Int to_int() const          { return get_object().to_int(); }
    UInt to_uint() const        { return get_object().to_uint(); }
    Float to_float() const      { return get_object().to_float(); }
    std::string to_str() const  { return get_object().to_str(); }
    Key to_key() const          { return get_object().to_key(); }
    std::string to_json() const { return get_object().to_json(); }

    template <typename Arg>
    Access<Key> operator [] (Arg&& arg);

    template <typename Arg>
    const Access<Key> operator [] (Arg&& arg) const;

    template <typename Arg>
    Access<Key> get(Arg&& arg);

    template <typename Arg>
    const Access<Key> get(Arg&& arg) const;

    template <typename Arg>
    void set(Arg&& arg, const Node& node);

    Node get_immed(const Key& key);
    const Node get_immed(const Key& key) const;

    bool operator == (const Node& other) const  { return get_object() == other.get_object(); }
    auto operator <=> (const Node& other) const { return get_object() <=> other.get_object(); }

    Oid id() const                { return get_object().id(); }
    size_t hash() const           { return get_object().hash(); }
    refcnt_t ref_count() const { return get_object().ref_count(); }

    operator Object () const { return get_object(); }
    operator Object ()       { return get_object(); }

  protected:
    Node(Node* p_parent, const Object& object) : object{object}, r_parent{p_parent} {}
    Node(Node* p_parent, Object&& object) : object{std::move(object)}, r_parent{p_parent} {}

    const Object& get_object() const {
        Node& self = *const_cast<Node*>(this);
        if (r_store && !r_store.is_synced(self)) {
            self.object = r_store->read(self);
        }
        return object;
    }

    Object& get_object() {
        if (r_store && !r_store.is_synced(*this)) {
            object = r_store->read(*this);
        }
        return object;
    }

    Node& replace(const Node& other);
    Node& replace(Node&& other);

  private:
    Object object;
    impl::NodeRef r_parent;
    impl::DataStoreRef r_store;

    friend class impl::NodeRef;
    friend class impl::DataStoreRef;
    friend class IDataStore;
    friend class Access<Key>;
};


//class OPath
//{
//  public:
//    explicit OPath(const char *path) : path{path}, delimiter{'.'} {}
//    explicit OPath(std::string&& path) : path{std::forward<std::string>(path)}, delimiter{'.'} {}
//
//    OPath(const OPath&) = default;
//    OPath(OPath&&) = default;
//
//    const Node lookup(const Node& origin) const { return lookup_impl(origin); }
//    Node lookup(Node& origin) const { return lookup_impl(origin); }
//
//    const char delim() const { return delimiter; }
//
//  private:
//    template <typename T>
//    T lookup_impl(T& origin) const;
//
//  private:
//    const std::string path;
//    const char delimiter;
//};


template <typename T>
class Access
{
  public:
    Node& operator = (const Node& node);
    Node& operator = (Node&& node);

    const Node parent() const { return finish().parent(); }
    Node parent()             { return finish().parent(); }

    const Key key() const                     { return finish().key(); }
    const Key key_of(const Node& child) const { return finish().key_of(); }

    template <class StoreClass, typename ... Args>
    void bind(Args&& ... args) { return finish().template bind<StoreClass>(std::forward<Args>(args) ...); }

    bool has_data_store() const { return finish().has_data_source(); }

    bool is_null() const        { return finish().is_null(); }
    bool is_bool() const        { return finish().is_bool(); }
    bool is_int() const         { return finish().is_int(); }
    bool is_uint() const        { return finish().is_uint(); }
    bool is_float() const       { return finish().is_float(); }
    bool is_str() const         { return finish().is_str(); }
    bool is_num() const         { return finish().is_num(); }
    bool is_list() const        { return finish().is_list(); }
    bool is_map() const         { return finish().is_map(); }
    bool is_container() const   { return finish().is_container(); }

    Int& as_int()         { return finish().as_int(); }
    UInt& as_uint()       { return finish().as_uint(); }
    Float& as_fp()        { return finish().as_fp(); }
    std::string& as_str() { return finish().as_str(); }

    Int as_int() const                { return finish().as_int(); }
    UInt as_uint() const              { return finish().as_uint(); }
    Float as_fp() const               { return finish().as_fp(); }
    std::string const& as_str() const { return finish().as_str(); }

    bool to_bool() const        { return finish().to_bool(); }
    Int to_int() const          { return finish().to_int(); }
    UInt to_uint() const        { return finish().to_uint(); }
    Float to_fp() const         { return finish().to_fp(); }
    std::string to_str() const  { return finish().to_str(); }
    Key to_key() const          { return finish().to_key(); }
    std::string to_json() const { return finish().to_json(); }

    template <typename Arg>
    Access<Key> operator [] (Arg&& arg);

    template <typename Arg>
    const Access<Key> operator [] (Arg&& arg) const;

    template <typename Arg>
    Access<Key> get(Arg&& arg);

    template <typename Arg>
    const Access<Key> get(Arg&& arg) const;

    bool operator == (const Node& other) const  { return finish().operator == (other); }
    auto operator <=> (const Node& other) const { return finish().operator <=> (other); }

    Oid id() const           { return finish().id(); }
    size_t hash() const      { return finish().hash(); }
    refcnt_t ref_count() const { return finish().ref_count(); }

  protected:
    Access(Node& node, const T& spec) : node{node}, spec{spec} {}
    Access(Node&& node, const T& spec) : node{std::forward<Node>(node)}, spec{spec} {}

    Access(Node& node, T&& spec) : node{node}, spec{std::move(spec)} {}
    Access(Node&& node, T&& spec) : node{std::forward<Node>(node)}, spec{std::move(spec)} {}

    Access& operator = (const Access& access) {
//        fmt::print("Access::operator=(const Access&)\n");
        node = access.node;
        spec = access.spec;
        return *this;
    }

    Access& operator = (Access&& access) {
//        fmt::print("Access::operator=(const Access&&)\n");
        node = std::move(access.node);
        spec = std::move(access.spec);
        return *this;
    }

    Node finish() const {
//        fmt::print("Access<T>::finish() node={}\n", node.id().to_str());
        Node& self = *const_cast<Node*>(&node);
        if (self.r_store && !self.r_store.is_synced(self, spec)) {
            self.object = self.r_store->read(self, spec);
        }
        return {&self, self.object.get(spec)};
    }

  private:
    Node node;
    const T spec;

    friend class Node;
};


template <typename T>
Node& Access<T>::operator = (const Node& other) {
//    fmt::print("Access::operator=(const Node&) <- {}\n", other.to_str());
    auto& r_store = node.r_store;
    if (r_store && !r_store.is_synced(node, spec)) {
        r_store->write(node, spec, other);
    } else {
        if (node.r_parent.is_null()) {
            node.object = other.get_object();
        } else {
            node.replace(other);
        }
    }
    return node;
}

template <typename T>
Node& Access<T>::operator = (Node&& other) {
//    fmt::print("Access::operator=(Node&&) <- {}\n", other.to_str());
    auto& r_store = node.r_store;
    if (r_store && !r_store.is_synced(node, spec)) {
        r_store->write(node, spec, std::forward<Node>(other));
    } else {
        if (node.r_parent.is_null()) {
            node.object = std::move(other.get_object());
        } else {
            node.replace(other);
        }
    }
    return node;
}

template <typename T>
template <typename Arg>
Access<Key> Access<T>::operator [] (Arg&& arg) {
    return get(std::forward<Arg>(arg));
}

template <typename T>
template <typename Arg>
const Access<Key> Access<T>::operator [] (Arg&& arg) const {
    return get(std::forward<Arg>(arg));
}

template <typename T>
template <typename Arg>
Access<Key> Access<T>::get(Arg&& arg) {
    return {finish(), Key{std::forward<Arg>(arg)}};
}

template <typename T>
template <typename Arg>
const Access<Key> Access<T>::get(Arg&& arg) const {
    return {finish(), std::forward<Arg>(arg)};
}

inline
Node::Node(const Access<Key>& access) : Node{access.finish()} {
//    fmt::print("Node(const Access<Key>&)\n");
}

inline
Node::Node(Access<Key>&& access) : Node{std::forward<Access<Key>>(access).finish()} {
    fmt::print("Node(Access<Key>&&)\n");
}

inline
Node& Node::operator = (const Access<Key>& other) {
    return this->operator = (other.finish());
}

inline
Node& Node::operator = (Access<Key>&& other) {
    return this->operator = (other.finish());
}

inline
const Key Node::key_of(const Node& node) const {
    if (object.is_list()) {
        Oid node_id = node.object.id();
        const List& list = object.unsafe_as<List>();
        UInt index = 0;
        for (const auto& item : list) {
            if (item.id() == node_id)
                return Key{index};
            index++;
        }
    } else if (object.is_map()) {
        Oid node_id = node.object.id();
        const Map& map = object.unsafe_as<Map>();
        for (auto& [key, value] : map) {
            if (value.id() == node_id)
                return key;
        }
    }

    return Key{};
}

inline
const Key Node::key() const {
    if (r_parent) {
        return r_parent->key_of(object);
    }
    return Key{};
}

template <typename Arg>
Access<Key> Node::operator [] (Arg&& arg) {
    return get(std::forward<Arg>(arg));
}

template <typename Arg>
const Access<Key> Node::operator [] (Arg&& arg) const {
    return get(std::forward<Arg>(arg));
}

template <typename Arg>
Access<Key> Node::get(Arg&& arg) {
    return {*this, std::forward<Arg>(arg)};
}

template <typename Arg>
const Access<Key> Node::get(Arg&& arg) const {
    return {*const_cast<Node*>(this), std::forward<Arg>(arg)};
}

inline
Node Node::get_immed(const Key& key) {
//    fmt::print("Node::get_immed({})\n", key.to_str());
    if (r_store && !r_store.is_synced(*this, key)) {
        object = r_store->read(*this, key);
    }
    return {this, object.get(key)};
}

inline
const Node Node::get_immed(const Key& key) const {
//    fmt::print("Node::get_immed({})\n", key.to_str());
    Node& self = *const_cast<Node*>(this);
    if (r_store && !r_store.is_synced(self, key)) {
        self.object = r_store->read(self, key);
    }
    return {&self, object.get(key)};
}

inline
Node& Node::replace(const Node& other) {
    Object parent_obj = parent().object;
    switch (parent_obj.repr_ix) {
        case Object::LIST_I: {
            List& list = parent_obj.as<List>();
            list[key().to_uint()] = other.get_object();
            break;
        }
        case Object::OMAP_I: {
            Map& map = parent_obj.as<Map>();
            map[key()] = other.get_object();
            break;
        }
        default:
            throw Object::wrong_type(parent_obj.repr_ix);
    }
    return *this;
}

inline
Node& Node::replace(Node&& other) {
    Object parent_obj = parent().object;
    switch (parent_obj.repr_ix) {
        case Object::LIST_I: {
            List& list = parent_obj.as<List>();
            list[key().to_uint()] = std::move(other.get_object());
            break;
        }
        case Object::OMAP_I: {
            Map& map = parent_obj.as<Map>();
            map[key()] = std::move(other.get_object());
            break;
        }
        default:
            throw Object::wrong_type(parent_obj.repr_ix);
    }
    return *this;
}

//template <typename T>
//T OPath::lookup_impl(T& origin) const {
//    std::remove_cv_t<T> node = origin;
//
//    char *a = const_cast<char*>(path.c_str());
//    char *end = a + path.length();
//    while (a != end) {
//        char* b = a;
//        for (; b != end && *b != delim(); b++);
//
//        if (b == end) {
//            node = node.get(a);
//        } else {
//            char c = *b;
//            *b = '\0';
//            try {
//                node = node.get(a);
//                *b = c;
//            }
//            catch (...) {
//                *b = c;
//                throw;
//            }
//        }
//
//        a = b;
//    }
//
//    return node;
//}

inline
bool IDataStore::is_synced(Node& node) {
    // TODO: add another variant to Datum to indicate an unloaded object instead of using null
    return node.object.is_null();
}

inline
bool IDataStore::is_synced(Node& node, const Key& key) {
    // TODO: add another variant to Datum to indicate an unloaded object instead of using null
    return (sync_mode == KEY)? is_key_synced(node, key): node.object.is_null();
}


namespace impl {

inline
void NodeRef::set(const Node& p_const_node) {
    Node* new_ptr = new Node{const_cast<Node&>(p_const_node)};
    Node* old_ptr = ptr;
    ptr = new_ptr;
    if (ptr != nullptr) ptr->object.inc_ref_count();
    if (old_ptr != nullptr && old_ptr->object.dec_ref_count()) {
        delete old_ptr;
    }
}

inline
void NodeRef::reset() {
    if (ptr != nullptr) {
        ptr->object.dec_ref_count();
        ptr = nullptr;
    }
}

inline
bool DataStoreRef::is_synced(Node& node) const {
    return ptr != nullptr && node.object.is_null();
}

template <typename Arg>
bool DataStoreRef::is_synced(Node& node, Arg&& arg) const {
    return ptr != nullptr && ptr->is_synced(node, std::forward<Arg>(arg));
}

} // namespace impl
} // namespace nodel
