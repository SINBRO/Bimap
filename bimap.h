#pragma once

#include "treap.h"
#include <cstddef>
#include <stdexcept>
#include <type_traits>

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap {
  using left_t = Left;
  using right_t = Right;

  struct tag_right;
  struct tag_left;

  using node_left = node_tree<left_t, tag_left>;
  using node_right = node_tree<right_t, tag_right>;

private:
  template <typename Tag>
  static constexpr bool is_left = std::is_same_v<Tag, tag_left>;

  template <typename Tag>
  using key_t = std::conditional_t<is_left<Tag>, left_t, right_t>;

  template <typename Tag> using node_ = node_tree<key_t<Tag>, Tag>;

  template <typename Tag>
  using tag_other = std::conditional_t<is_left<Tag>, tag_right, tag_left>;

  template <typename Tag>
  using compare_t = std::conditional_t<is_left<Tag>, CompareLeft, CompareRight>;

  template <typename Tag>
  using treap_t = treap<key_t<Tag>, compare_t<Tag>, Tag>;

  template <typename Tag> using key_t_other = key_t<tag_other<Tag>>;

  template <typename Tag> using node_other = node_<tag_other<Tag>>;

  // base exists to allow storing fake element without storing keys for it
  struct node_t_base : node_left, node_right {
    node_t_base() = default;
  };

  struct node_t : node_t_base {
    left_t key_left;
    right_t key_right;

    node_t(left_t l, right_t r)
        : key_left(std::move(l)), key_right(std::move(r)) {}
  };

  // note: I know that lambdas should be generally faster for this, but I faced
  // what appears to be a gcc bug very similar to this:
  // http://www.cplusplus.com/forum/beginner/130564/
  // btw tests are passing ~5% slower, so doesn't look like a big problem
  template <typename Tag> static key_t<Tag> &key_getter(node_<Tag> *node) {
    if constexpr (is_left<Tag>) {
      return static_cast<node_t *>(node)->key_left;
    } else {
      return static_cast<node_t *>(node)->key_right;
    }
  }

  template <typename Tag> struct iterator {
    using node_it = node_tree<key_t<Tag>, Tag>;
    using iterator_other = iterator<tag_other<Tag>>;

    node_it *node = nullptr;

    explicit iterator(node_it *node) : node(node) {}

    iterator(const iterator &other) : node(other.node) {}

    iterator &operator=(const iterator &other) {
      node = other.node;
      return *this;
    }

    // Далее в комментариях итератора вместо left можно подставить key_t
    // Элемент на который сейчас ссылается итератор.
    // Разыменование итератора end_left() неопределено.
    // Разыменование невалидного итератора неопределено.
    key_t<Tag> const &operator*() const noexcept {
      return key_getter<Tag>(node);
    }

    // Переход к следующему по величине left'у.
    // Инкремент итератора end_left() неопределен.
    // Инкремент невалидного итератора неопределен.
    iterator &operator++() {
      node = node->next();
      return *this;
    }

    iterator operator++(int) {
      auto res = *this;
      node = node->next();
      return res;
    }

    // Переход к предыдущему по величине left'у.
    // Декремент итератора begin_left() неопределен.
    // Декремент невалидного итератора неопределен.
    iterator &operator--() {
      node = node->prev();
      return *this;
    }

    iterator operator--(int) {
      auto res = *this;
      node = node->prev();
      return res;
    }

    // left_iterator ссылается на левый элемент некоторой пары.
    // Эта функция возвращает итератор на правый элемент той же пары.
    // end_left().flip() возращает end_right().
    // end_right().flip() возвращает end_left().
    // flip() невалидного итератора неопределен.
    iterator_other flip() const noexcept {
      return iterator_other(
          static_cast<node_other<Tag> *>(static_cast<node_t_base *>(node)));
    }

    bool operator==(iterator const &other) const noexcept {
      return node == other.node;
    };

    bool operator!=(iterator const &other) const noexcept {
      return node != other.node;
    }
  };

public:
  using left_iterator = iterator<tag_left>;
  using right_iterator = iterator<tag_right>;

  // Создает bimap не содержащий ни одной пары.
  explicit bimap(CompareLeft compare_left = CompareLeft(),
                 CompareRight compare_right = CompareRight())
      : treap_left(std::move(compare_left),
                   static_cast<node_left *>(&end_node)),
        treap_right(std::move(compare_right),
                    static_cast<node_right *>(&end_node)) {
    validate_ends();
  };

  // Конструкторы от других и присваивания
  bimap(bimap const &other)
      : treap_left(std::move(other.treap_left.get_cmp()),
                   static_cast<node_left *>(&end_node)),
        treap_right(std::move(other.treap_right.get_cmp()),
                    static_cast<node_right *>(&end_node)) {
    try {
      validate_ends();
      insert_all(other);
    } catch (...) {
      erase_all();
      throw;
    }
  };

  bimap(bimap &&other) noexcept
      : treap_left(std::move(other.treap_left)),
        treap_right(std::move(other.treap_right)), size_(other.size_) {
    validate_ends();
    other.validate_ends();
  };

  bimap &operator=(const bimap &other) {
    if (&other == this) {
      return *this;
    }

    bimap copied = bimap(other); // can be an exception here
    *this = std::move(copied);   // no exceptions here
    return *this;

  };

  bimap &operator=(bimap &&other) noexcept {
    treap_left = std::move(other.treap_left); // is full swap
    treap_right = std::move(other.treap_right);
    std::swap(size_, other.size_);
    return *this;
  };

  // Деструктор. Вызывается при удалении объектов bimap.
  // Инвалидирует все итераторы ссылающиеся на элементы этого bimap
  // (включая итераторы ссылающиеся на элементы следующие за последними).
  ~bimap() { erase_all(); };

  // Вставка пары (left, right), возвращает итератор на left.
  // Если такой left или такой right уже присутствуют в bimap, вставка не
  // производится и возвращается end_left().
  left_iterator insert(left_t const &left, right_t const &right) {
    return insert_impl(left, right);
  }

  left_iterator insert(left_t const &left, right_t &&right) {
    return insert_impl(left, std::move(right));
  }

  left_iterator insert(left_t &&left, right_t const &right) {
    return insert_impl(std::move(left), right);
  }

  left_iterator insert(left_t &&left, right_t &&right) {
    return insert_impl(std::move(left), std::move(right));
  }

  // Удаляет элемент и соответствующий ему парный.
  // erase невалидного итератора неопределен.
  // erase(end_left()) и erase(end_right()) неопределены.
  // Пусть it ссылается на некоторый элемент e.
  // erase инвалидирует все итераторы ссылающиеся на e и на элемент парный к e.
  left_iterator erase_left(left_iterator it) {
    return erase_impl<tag_left>(it);
  }

  // Аналогично erase, но по ключу, удаляет элемент если он присутствует, иначе
  // не делает ничего Возвращает была ли пара удалена
  bool erase_left(left_t const &left) {
    return erase_impl<tag_left>(left, treap_left);
  };

  right_iterator erase_right(right_iterator it) {
    return erase_impl<tag_right>(it);
  }

  bool erase_right(right_t const &right) {
    return erase_impl<tag_right>(right, treap_right);
  }

  // erase от ренжа, удаляет [first, last), возвращает итератор на последний
  // элемент за удаленной последовательностью
  left_iterator erase_left(left_iterator first, left_iterator last) {
    return erase_impl<tag_left>(first, last);
  }

  right_iterator erase_right(right_iterator first, right_iterator last) {
    return erase_impl<tag_right>(first, last);
  }

  // Возвращает итератор по элементу. Если не найден - соответствующий end()
  left_iterator find_left(left_t const &left) const {
    return left_iterator(treap_left.find(left, &key_getter<tag_left>));
  }

  right_iterator find_right(right_t const &right) const {
    return right_iterator(treap_right.find(right, &key_getter<tag_right>));
  }

private:
  template <typename Tag> iterator<Tag> find(const key_t<Tag> &key) const {
    if constexpr (is_left<Tag>) {
      return find_left(key);
    } else {
      return find_right(key);
    }
  }

  template <typename Tag> iterator<Tag> end() const {
    if constexpr (is_left<Tag>) {
      return end_left();
    } else {
      return end_right();
    }
  }

public:
  // Возвращает противоположный элемент по элементу
  // Если элемента не существует -- бросает std::out_of_range
  right_t const &at_left(left_t const &key) const { return at<tag_left>(key); }

  left_t const &at_right(right_t const &key) const {
    return at<tag_right>(key);
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует, добавляет его в bimap и на противоположную
  // сторону кладет дефолтный элемент, ссылку на который и возвращает
  // Если дефолтный элемент уже лежит в противоположной паре - должен поменять
  // соответствующий ему элемент на запрашиваемый (смотри тесты)
  template <typename T = right_t,
            typename = std::enable_if_t<std::is_default_constructible_v<T>>>
  right_t const &at_left_or_default(left_t const &key) {
    return at_or_default<tag_left>(key);
  }

  template <typename T = left_t,
            typename = std::enable_if_t<std::is_default_constructible_v<T>>>
  left_t const &at_right_or_default(right_t const &key) {
    return at_or_default<tag_right>(key);
  }

  // lower и upper bound'ы по каждой стороне
  // Возвращают итераторы на соответствующие элементы
  // Смотри std::lower_bound, std::upper_bound.
  left_iterator lower_bound_left(const left_t &left) const {
    return left_iterator(treap_left.lower_bound(left, &key_getter<tag_left>));
  };

  left_iterator upper_bound_left(const left_t &left) const {
    return left_iterator(treap_left.upper_bound(left, &key_getter<tag_left>));
  }

  right_iterator lower_bound_right(const right_t &right) const {
    return right_iterator(
        treap_right.lower_bound(right, &key_getter<tag_right>));
  }

  right_iterator upper_bound_right(const right_t &right) const {
    return right_iterator(
        treap_right.upper_bound(right, &key_getter<tag_right>));
  }

  // Возващает итератор на минимальный по порядку left.
  left_iterator begin_left() const noexcept {
    return left_iterator(treap_left.first());
  }
  // Возващает итератор на следующий за последним по порядку left.
  left_iterator end_left() const noexcept {
    return left_iterator(treap_left.last());
  }

  // Возващает итератор на минимальный по порядку right.
  right_iterator begin_right() const noexcept {
    return right_iterator(treap_right.first());
  }
  // Возващает итератор на следующий за последним по порядку right.
  right_iterator end_right() const noexcept {
    return right_iterator(treap_right.last());
  }

  // Проверка на пустоту
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  // Возвращает размер бимапы (кол-во пар)
  [[nodiscard]] std::size_t size() const { return size_; }

  // операторы сравнения
  template <typename TL, typename TR, typename CompL, typename CompR>
  friend bool operator==(bimap const &a, bimap const &b);

  template <typename TL, typename TR, typename CompL, typename CompR>
  friend bool operator!=(bimap const &a, bimap const &b);

private:
  template <typename Tag> bool contains(const key_t<Tag> &key) const noexcept {
    if constexpr (is_left<Tag>) {
      return find_left(key) != end_left();
    } else {
      return find_right(key) != end_right();
    }
  }

  template <typename Tag>
  key_t_other<Tag> const &at(const key_t<Tag> &key) const {
    const treap_t<Tag> *treap_;
    if constexpr (std::is_same_v<Tag, tag_left>) {
      treap_ = &treap_left;
    } else {
      treap_ = &treap_right;
    }
    auto res = treap_->find(key, &key_getter<Tag>);
    if (treap_->is_last(res)) {
      throw std::out_of_range("Key not found");
    }
    return key_getter<tag_other<Tag>>(
        static_cast<node_other<Tag> *>(static_cast<node_t_base *>(res)));
  }

  template <typename Tag>
  key_t_other<Tag> const &at_or_default(const key_t<Tag> &key) {
    if (contains<Tag>(key)) {
      return at<Tag>(key);
    }
    key_t_other<Tag> val = key_t_other<Tag>();
    iterator<tag_other<Tag>> it = find<tag_other<Tag>>(val);
    if (it != end<tag_other<Tag>>()) {
      erase_impl<tag_other<Tag>>(it);
    }
    if constexpr (is_left<Tag>) {
      return *insert(key, val).flip();
    } else {
      return *insert(val, key);
    }
  }

  void erase_all() { erase_left(begin_left(), end_left()); }

  void insert_all(const bimap &other) {
    for (left_iterator iter = other.begin_left(); iter != other.end_left();
         ++iter) {
      insert(*iter, *iter.flip());
    }
  }

  template <typename T1, typename T2>
  left_iterator insert_impl(T1 &&left, T2 &&right) {
    if (contains<tag_left>(left) || contains<tag_right>(right)) {
      return end_left();
    }
    auto *node = new node_t(std::forward<T1>(left), std::forward<T2>(right));
    treap_left.insert(node, &key_getter<tag_left>);
    treap_right.insert(node, &key_getter<tag_right>);
    ++size_;
    return left_iterator(node);
  }

  template <typename Tag> iterator<Tag> erase_impl(iterator<Tag> iter) {
    auto *ptr = static_cast<node_t *>(iter.node);
    auto *res_r = treap_right.remove(static_cast<node_right *>(ptr));
    auto *res_l = treap_left.remove(static_cast<node_left *>(ptr));
    delete ptr;
    --size_;
    if constexpr (is_left<Tag>) {
      return iterator<Tag>(static_cast<node_<Tag> *>(res_l));
    } else {
      return iterator<Tag>(static_cast<node_<Tag> *>(res_r));
    }
  }

  template <typename Tag>
  bool erase_impl(const key_t<Tag> &key, treap_t<Tag> &tree) {
    auto *node = tree.find(key, &key_getter<Tag>);
    if (tree.is_last(node)) {
      return false;
    }
    erase_impl<Tag>(iterator<Tag>(node));
    return true;
  }

  template <typename Tag>
  iterator<Tag> erase_impl(iterator<Tag> first, iterator<Tag> last) {
    while (first != last) {
      first = erase_impl<Tag>(first);
    }
    return last;
  }

  void validate_ends() noexcept {
    validate_end(static_cast<node_left *>(&end_node), treap_left);
    validate_end(static_cast<node_right *>(&end_node), treap_right);
  }

  template <typename Tag>
  void validate_end(node_<Tag> *base, treap_t<Tag> &tree) noexcept {
    base->parent = nullptr;
    base->right = nullptr;
    base->left = tree.root();
    tree.storage.end_elem = base;
    tree.set_parent(tree.root(), base);
  }

  treap<left_t, CompareLeft, tag_left> treap_left;
  treap<right_t, CompareRight, tag_right> treap_right;
  size_t size_ = 0;
  node_t_base end_node;
};

template <typename TL, typename TR, typename CompL, typename CompR>
bool operator==(const bimap<TL, TR, CompL, CompR> &map1,
                const bimap<TL, TR, CompL, CompR> &map2) {
  if (map1.size() != map2.size()) {
    return false;
  }
  auto it1 = map1.begin_left();
  auto it2 = map2.begin_left();
  while (it1 != map1.end_left()) { // same size
    if (*it1 != *it2 || *it1.flip() != *it2.flip()) {
      return false;
    }
    ++it1;
    ++it2;
  }
  return true;
}

template <typename TL, typename TR, typename CompL, typename CompR>
bool operator!=(const bimap<TL, TR, CompL, CompR> &map1,
                const bimap<TL, TR, CompL, CompR> &map2) {
  return !(map1 == map2);
}
