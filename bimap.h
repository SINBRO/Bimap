#pragma once

template <typename Left, typename Right, typename CompareLeft, typename CompareRight>
struct bimap {
    using left_t  = ...;
    using right_t = ...;

    using node_t = ...;

    struct right_iterator; // По аналогии с left_iterator
    struct left_iterator {
        // Элемент на который сейчас ссылается итератор.
        // Разыменование итератора end_left() неопределено.
        // Разыменование невалидного итератора неопределено.
        left_t const& operator*() const;

        // Переход к следующему по величине left'у.
        // Инкремент итератора end_left() неопределен.
        // Инкремент невалидного итератора неопределен.
        left_iterator& operator++();
        left_iterator operator++(int);

        // Переход к предыдущему по величине left'у.
        // Декремент итератора begin_left() неопределен.
        // Декремент невалидного итератора неопределен.
        left_iterator& operator--();
        left_iterator operator--(int);

        // left_iterator ссылается на левый элемент некоторой пары.
        // Эта функция возвращает итератор на правый элемент той же пары.
        // end_left().flip() возращает end_right().
        // end_right().flip() возвращает end_left().
        // flip() невалидного итератора неопределен.
        right_iterator flip() const;
    };

    // Создает bimap не содержащий ни одной пары.
    bimap() noexcept;

    // Конструкторы от других и присваивания
    bimap(bimap const& other) = delete;
    bimap(bimap&& other) noexcept;

    bimap& operator=(bimap const& other) = delete;
    bimap& operator=(bimap&& other) noexcept;

    // Деструктор. Вызывается при удалении объектов bimap.
    // Инвалидирует все итераторы ссылающиеся на элементы этого bimap
    // (включая итераторы ссылающиеся на элементы следующие за последними).
    ~bimap();

    // Вставка пары (left, right), возвращает итератор на left.
    // Если такой left или такой right уже присутствуют в bimap, вставка не
    // производится и возвращается end_left().
    left_iterator insert(left_t const& left, right_t const& right);
    left_iterator insert(left_t const& left, right_t&& right);
    left_iterator insert(left_t&& left, right_t const& right);
    left_iterator insert(left_t&& left, righ_t&& right);

    // Удаляет элемент и соответствующий ему парный.
    // erase невалидного итератора неопределен.
    // erase(end_left()) и erase(end_right()) неопределены.
    // Пусть it ссылается на некоторый элемент e.
    // erase инвалидирует все итераторы ссылающиеся на e и на элемент парный к e.
    left_iterator erase_left(left_iterator it);
    // Аналогично erase, но по ключу, удаляет элемент если он присутствует, иначе не делает ничего
    // Возвращает была ли пара удалена
    bool erase_left(left_t const& left);

    right_iterator erase_right(right_iterator it);
    bool erase_right(right_t const& right);

    // Возвращает итератор по элементу. Если не найден - соответствующий end()
    left_iterator  find_left (left_t  const& left)  const;
    right_iterator find_right(right_t const& right) const;

    // Возвращает противоположный элемент по элементу
    // Если элемента не существует -- бросает std::out_of_range
    right_t const& at_left(left_t const& key) const;
    left_t const& at_right(right_t const& key) const;

    right_t at_left_or_default(left_t const& key) const;
    left_t at_right_or_default(right_t const& key) const;

    // lower и upper bound'ы по каждой стороне
    // Возвращают итераторы на соответствующие элементы
    // Смотри std::lower_bound, std::upper_bound.
    left_iterator lower_bound_left(const left_t& left) const;
    left_iterator upper_bound_left(const left_t& left) const;

    right_iterator lower_bound_right(const right_t& left) const;
    right_iterator upper_bound_right(const right_t& left) const;

    // Возващает итератор на минимальный по величине left.
    left_iterator begin_left() const;
    // Возващает итератор на следующий за последним по величине left.
    left_iterator end_left() const;

    // Возващает итератор на минимальный по величине right.
    right_iterator begin_right() const;
    // Возващает итератор на следующий за последним по величине right.
    right_iterator end_right() const;

    // Проверка на пустоту
    bool empty() const;

    // Возвращает размер бимапы (кол-во пар)
    std::size_t size() const;

    // операторы сравнения
    friend bool operator==(bimap const& a, bimap const& b);
    friend bool operator!=(bimap const& a, bimap const& b);
    // операторы >, < ???????? (лексикографическое сравнение)

    // ==== как бонус если делать нормально ====
    // erase от ренжа
    left_iterator erase_left(left_iterator first, left_iterator last);
    right_iterator erase_right(right_iterator first, right_iterator last);
};
