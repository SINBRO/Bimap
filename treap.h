#pragma once

#include <random>
#include <stdexcept>

template <typename T, typename Tag> struct node_tree {
  node_tree *parent = nullptr;
  node_tree *left = nullptr;
  node_tree *right = nullptr;

  T key;

  int priority = rand();

  explicit node_tree(T key) : key(std::move(key)) {}

  // explicit node_tree(T&& key) : key(std::move(key)) {}


  node_tree *next() noexcept {
    node_tree *cur = this;
    if (cur->right == nullptr) {
      auto par = cur->parent;
      while (par != nullptr && par->right == cur) {
        cur = par;
        par = cur->parent;
      }
      return par;
    }
    cur = cur->right;
    while (cur->left != nullptr) {
      cur = cur->left;
    }
    return cur;
  }

  node_tree *prev() noexcept {
    node_tree *cur = this;
    if (cur->left == nullptr) {
      auto par = cur->parent;
      while (par != nullptr && par->left == cur) {
        cur = par;
        par = cur->parent;
      }
      return par;
    }
    cur = cur->left;
    while (cur->right != nullptr) {
      cur = cur->right;
    }
    return cur;
  }

};

template <typename T, typename Comparator, typename Tag> struct treap {
  using node_t_ = node_tree<T, Tag>;

  Comparator cmp = Comparator();
  node_t_ *root = nullptr;
  // this is to have a common "end" node for all trees
  // memory control is on treap user
  node_t_ *end_elem = nullptr;

  treap() = default;

  explicit treap(Comparator comparator, node_t_* end) : cmp(comparator), end_elem(end) { init_end(); }


  treap(treap &&other) noexcept : cmp(std::move(other.cmp)) {
    std::swap(root, other.root);
    std::swap(end_elem, other.end_elem);
    init_end();
    other.init_end();
  }

  treap &operator=(treap &&other) noexcept {
    std::swap(root, other.root);
    std::swap(end_elem, other.end_elem);
    cmp = std::move(other.cmp);
    reconnect_end();
    other.reconnect_end();
    return *this;
  }

  bool less(const T &key1, const T &key2) const noexcept {
    return cmp(key1, key2);
  }

  static inline void set_parent(node_t_ *child, node_t_ *parent) noexcept {
    if (child != nullptr)
      child->parent = parent;
  }

  // a "static" operation (uses only cmp from *this)
  std::pair<node_t_ *, node_t_ *> split(node_t_ *node, const T &key) noexcept {
    if (node == nullptr) {
      return std::pair(nullptr, nullptr);
    }
    if (less(node->key, key)) { // (key > node->key) {
      auto split_r = split(node->right, key);
      node->right = split_r.first;
      set_parent(split_r.first, node);
      set_parent(split_r.second, nullptr);
      return std::pair(node, split_r.second);
    }
    auto split_l = split(node->left, key);
    node->left = split_l.second;
    set_parent(split_l.second, node);
    set_parent(split_l.first, nullptr);
    return std::pair(split_l.first, node);
  }

  // all keys in tree1 are expected to be < than all keys in tree2
  static node_t_ *merge(node_t_ *tree1, node_t_ *tree2) {
    if (tree2 == nullptr)
      return tree1;
    if (tree1 == nullptr)
      return tree2;

    if (tree1->priority > tree2->priority) {
      tree1->right = merge(tree1->right, tree2);
      set_parent(tree1->right, tree1);
      return tree1;
    }
    tree2->left = merge(tree1, tree2->left);
    set_parent(tree2->left, tree2);
    return tree2;
  }

  // a "static" operation (uses only cmp from this)
  // returns "new" treap;  non null node expected
  node_t_ *insert(node_t_ *tree, node_t_ *node) noexcept {
    if (tree == nullptr)
      return node;
    if (node->priority > tree->priority) {
      auto split_tree = split(tree, node->key);
      node->left = split_tree.first;
      set_parent(split_tree.first, node);
      node->right = split_tree.second;
      set_parent(split_tree.second, node);
      return node;
    }
    if (less(node->key, tree->key)) {
      tree->left = insert(tree->left, node);
      tree->left->parent = tree;
      return tree;
    }
    tree->right = insert(tree->right, node);
    tree->right->parent = tree;
    return tree;
  }

  void insert(node_t_ *node) {
    root = insert(root, node);
    reconnect_end();
  }

  // node of the tree expected
  node_t_ *remove(node_t_ *tree, node_t_ *node) {
    node_t_ *par = node->parent;
    node_t_ *res = merge(node->left, node->right);
    set_parent(res, par);
    if (par == end_elem) {
      par->left = res;
      return res;
    }
    if (par->right == node) {
      par->right = res;
    } else {
      par->left = res;
    }
    return tree;
  }

  node_t_ *remove(node_t_ *node) {
    auto res = node->next();
    root = remove(root, node);
    reconnect_end();
    return res;
  }

  void remove(const T &key) {
    node_t_ *node = find(key);
    remove(node);
  }

  node_t_ *find(const T &key, node_t_ *node) const noexcept {
    while (node != nullptr) {
      if (less(node->key, key)) {
        node = node->right;
      } else if (less(key, node->key)) {
        node = node->left;
      } else {
        return node;
      }
    }
    return end_elem;
  }

  node_t_ *find(const T &key) const noexcept { return find(key, root); }

  node_t_ *lower_bound(const T &key, node_t_ *node) const noexcept {
    node_t_ *res = nullptr;
    while (node != nullptr) {
      if (!less(node->key, key)) {
        res = node;
        node = node->left;
      } else {
        node = node->right;
      }
    }
    if (res == nullptr) {
      return end_elem;
    }
    return res;
  }

  node_t_ *lower_bound(const T &key) const noexcept {
    return lower_bound(key, root);
  }

  node_t_ *upper_bound(const T &key, node_t_ *node) const noexcept {
    node_t_ *res = nullptr;
    while (node != nullptr) {
      if (less(key, node->key)) {
        res = node;
        node = node->left;
      } else {
        node = node->right;
      }
    }
    if (res == nullptr) {
      return end_elem;
    }
    return res;
  }

  node_t_ *upper_bound(const T &key) const noexcept {
    return upper_bound(key, root);
  }

  node_t_ *first() const noexcept {
    auto res = end_elem;
    while (res->left != nullptr) {
      res = res->left;
    }
    return res;
  }

  node_t_ *last() const noexcept {
    return end_elem;
  }

  inline bool is_last(node_t_ const *node) const noexcept {
    return end_elem == node;
  }

  //  ~treap() {
  //    delete root;
  //  }

private:
  void init_end() noexcept {
    end_elem->right = nullptr;
    end_elem->parent = nullptr;
    reconnect_end();
    end_elem->priority = std::numeric_limits<int>::max();
  }

  inline void reconnect_end() noexcept {
    end_elem->left = root;
    set_parent(root, end_elem);
  }
};
