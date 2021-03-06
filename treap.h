#pragma once

#include <limits>
#include <random>
#include <stdexcept>

int random_priority() {
  static std::default_random_engine generator(time(nullptr));
  static std::uniform_int_distribution<int> distribution;
  return distribution(generator);
}

template <typename T, typename Tag> struct node_tree {
  node_tree *parent = nullptr;
  node_tree *left = nullptr;
  node_tree *right = nullptr;

  int priority = random_priority();

  explicit node_tree() = default;

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

  // to implement empty base optimisation in case comparator is an empty class
  struct treap_storage : Comparator {
    // this is to have a common "end" node for all trees
    // memory control is on treap user
    node_t_ *end_elem = nullptr;

    treap_storage() : Comparator() {}

    treap_storage(treap_storage const &other) = default;

    treap_storage(treap_storage &&other) noexcept { swap(other); }

    treap_storage(Comparator comparator, node_t_ *end)
        : Comparator(std::move(comparator)), end_elem(end) {
      end_elem->left = nullptr;
      end_elem->right = nullptr;
      end_elem->parent = nullptr;
      end_elem->priority = std::numeric_limits<int>::max();
    }

    treap_storage &operator=(treap_storage &&other) noexcept {
      swap(other);
      return *this;
    }

    void swap(treap_storage &other) noexcept {
      std::swap<Comparator>(*this, other);
      std::swap(end_elem->left, other.end_elem->left);
    }
  };

  treap_storage storage;

  treap() = default;

  explicit treap(Comparator comparator, node_t_ *end)
      : storage(std::move(comparator), end) {}

  inline Comparator get_cmp() const { return static_cast<Comparator>(storage); }

  treap(treap &&other) noexcept : storage(std::move(other.storage)) {}

  treap &operator=(treap &&other) noexcept {
    storage.swap(other.storage);
    return *this;
  }

  void swap(treap &other) noexcept { storage.swap(other.storage); }

  inline node_t_ *root() const { return last()->left; }

  bool less(const T &key1, const T &key2) const noexcept {
    return (static_cast<const Comparator &>(storage))(key1, key2);
  }

  static inline void set_parent(node_t_ *child, node_t_ *parent) noexcept {
    if (child != nullptr)
      child->parent = parent;
  }

  // a "static" operation (uses only cmp from *this)
  template <typename Getter>
  std::pair<node_t_ *, node_t_ *> split(node_t_ *node, const T &key,
                                        Getter getter) noexcept {
    if (node == nullptr) {
      return std::pair(nullptr, nullptr);
    }
    if (less(getter(node), key)) { // (key > node->key)
      auto split_r = split(node->right, key, getter);
      node->right = split_r.first;
      set_parent(split_r.first, node);
      set_parent(split_r.second, nullptr);
      return std::pair(node, split_r.second);
    }
    auto split_l = split(node->left, key, getter);
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
  template <typename Getter>
  node_t_ *insert(node_t_ *tree, node_t_ *node, Getter getter) noexcept {
    if (tree == nullptr)
      return node;
    if (node->priority > tree->priority) {
      auto split_tree = split(tree, getter(node), getter);
      node->left = split_tree.first;
      set_parent(split_tree.first, node);
      node->right = split_tree.second;
      set_parent(split_tree.second, node);
      return node;
    }
    if (less(getter(node), getter(tree))) {
      tree->left = insert(tree->left, node, getter);
      tree->left->parent = tree;
      return tree;
    }
    tree->right = insert(tree->right, node, getter);
    tree->right->parent = tree;
    return tree;
  }

  template <typename Getter> void insert(node_t_ *node, Getter getter) {
    last()->left = insert(root(), node, getter);
    reconnect_end();
  }

  // node of the tree expected
  node_t_ *remove(node_t_ *tree, node_t_ *node) {
    node_t_ *par = node->parent;
    node_t_ *res = merge(node->left, node->right);
    set_parent(res, par);
    if (par == last()) {
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
    last()->left = remove(root(), node);
    reconnect_end();
    return res;
  }

  void remove(const T &key) {
    node_t_ *node = find(key);
    remove(node);
  }

  template <typename Getter>
  node_t_ *find(const T &key, node_t_ *node, Getter getter) const noexcept {
    while (node != nullptr) {
      if (less(getter(node), key)) {
        node = node->right;
      } else if (less(key, getter(node))) {
        node = node->left;
      } else {
        return node;
      }
    }
    return last();
  }

  template <typename Getter>
  node_t_ *find(const T &key, Getter getter) const noexcept {
    return find(key, root(), getter);
  }

  template <typename Getter>
  node_t_ *lower_bound(const T &key, node_t_ *node,
                       Getter getter) const noexcept {
    node_t_ *res = nullptr;
    while (node != nullptr) {
      if (!less(getter(node), key)) {
        res = node;
        node = node->left;
      } else {
        node = node->right;
      }
    }
    if (res == nullptr) {
      return last();
    }
    return res;
  }

  template <typename Getter>
  node_t_ *lower_bound(const T &key, Getter getter) const noexcept {
    return lower_bound(key, root(), getter);
  }

  template <typename Getter>
  node_t_ *upper_bound(const T &key, node_t_ *node,
                       Getter getter) const noexcept {
    node_t_ *res = nullptr;
    while (node != nullptr) {
      if (less(key, getter(node))) {
        res = node;
        node = node->left;
      } else {
        node = node->right;
      }
    }
    if (res == nullptr) {
      return last();
    }
    return res;
  }

  template <typename Getter>
  node_t_ *upper_bound(const T &key, Getter getter) const noexcept {
    return upper_bound(key, root(), getter);
  }

  node_t_ *first() const noexcept {
    auto res = last();
    while (res->left != nullptr) {
      res = res->left;
    }
    return res;
  }

  node_t_ *last() const noexcept { return storage.end_elem; }

  inline bool is_last(node_t_ const *node) const noexcept {
    return last() == node;
  }

private:
  inline void reconnect_end() noexcept { set_parent(last()->left, last()); }
};
