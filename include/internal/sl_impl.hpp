#pragma once

#include <iostream>
#include <functional>
#include <cassert>

namespace skip_list
{

namespace internal
{

template <typename SkipList> class sl_iterator;
template <typename SkipList> class sl_const_iterator;

template <typename T>
class sl_node
{
public:
    using value_type                = T;
    using size_type                 = std::size_t;
    using reference                 = value_type&;
    using const_reference           = const value_type&;
    using self_type = sl_node<T>;

    sl_node() = default;

    sl_node(value_type value, size_type level)
        : m_value(value)
        , m_level(level)
        , m_prev(nullptr)
        , m_next(new self_type*[level + 1])
    {
        for (size_type i = 0; i <= level; ++i) {
            m_next[i] = nullptr;
        }
    }

    ~sl_node()
    {
        delete [] m_next;
        m_prev = nullptr; // don't need this
        m_next = nullptr; // don't need this
    }

    value_type m_value;
    size_type m_level;
    self_type* m_prev;
    self_type** m_next; // node_type*[m_level + 1]

}; // sl_node

template <typename T,
          typename Compare,
          typename Allocator>
class sl_impl
{
private:
    using self = sl_impl<T, Compare, Allocator>;

public:
    // using value_type                = T;
    using size_type                   = std::size_t;
    using allocator_type              = Allocator;
    // using difference_type             = typename allocator_type::difference_type; // not sure
    using value_type                  = typename allocator_type::value_type;
    using reference                   = typename allocator_type::reference;
    using const_reference             = typename allocator_type::const_reference;
    using pointer                     = typename allocator_type::pointer;
    using const_pointer               = typename allocator_type::const_pointer;
    using compare                     = Compare;

    using level_type                  = std::size_t;

private:
    static inline level_type max_levels = 5; // make this a class member: = log(size)

public:
    using node_type = sl_node<T>;

    explicit sl_impl(const Allocator& alloc = Allocator())
        : m_alloc(alloc)
        , m_levels(max_levels)
        , m_size(0)
        , m_head(new node_type(std::numeric_limits<typename node_type::value_type>::min(), max_levels))
        , m_tail(new node_type(std::numeric_limits<typename node_type::value_type>::max(), max_levels))
    {
        for (size_type i = 0; i <= max_levels; ++i) {
            m_head->m_next[i] = m_tail;
            m_tail->m_next[i] = nullptr;
        }
        m_head->m_prev = nullptr;
        m_tail->m_prev = m_head;
    }

    ~sl_impl()
    {
        remove_all();
        delete m_head;
        delete m_tail;
    }

    allocator_type get_allocator() const { return m_alloc; }
    size_type size() const { return m_size; }

    node_type* front() { return m_head->m_next[0]; }
    const node_type* front() const { return m_head->m_next[0]; }

    node_type* back() { return m_tail->m_prev; }
    const node_type* back() const { return m_tail->m_prev; }

    node_type* head() { return m_head; }
    const node_type* head() const { return m_head; }
    node_type* tail() { return m_tail; }
    const node_type* tail() const { return m_tail; }

    node_type* find(const_reference value)
    {
        node_type* curr = m_head;
        for (level_type level = m_levels + 1; level > 0; ) {
            --level;
            while ((curr->m_next[level] != m_tail) &&
                    !is_equal(curr->m_value, value) &&
                    is_less_or_equal(curr->m_next[level]->m_value, value)) {
                curr = curr->m_next[level];
            }
        }
        return curr;
    }

    const node_type* find(const_reference value) const
    {
        return const_cast<sl_impl*>(this)->find(value);
    }

    node_type* find_first(const value_type& value)
    {
        node_type* node = find(value);
        if (!is_equal(node->m_value, value)) {
            node = node->m_next[0];
        }
        return node;
    }

    const node_type* find_first(const_reference value) const
    {
        return const_cast<sl_impl*>(this)->find_first(value);
    }

    [[nodiscard]] bool toss_a_coin() const
    {
        return rand() % 2 == 0;
    }

    [[nodiscard]] level_type random_level() const
    {
        level_type level = 1;
        while (level < max_levels && toss_a_coin()) {
            ++level;
        }
        return level;
    }

    node_type* insert(const value_type& value, node_type* hint = nullptr)
    {
        const level_type node_level = random_level();

        node_type* new_node = new node_type(value, node_level);
        const bool is_valid_hint = (hint != nullptr);

        node_type* curr = (is_valid_hint) ? hint : m_head; // for now start from head
        level_type level = curr->m_level + 1;
        do {
            --level;
            while ((curr->m_next[level] != m_tail) && m_less(curr->m_next[level]->m_value, value)) {
                curr = curr->m_next[level];
                assert(level <= curr->m_level);
            }
            if (level <= node_level) {
                assert(curr->m_next[level] != nullptr);
                new_node->m_next[level] = curr->m_next[level];
                curr->m_next[level] = new_node;
            }
        } while (level != 0);
        // level is 0
        assert(level == 0);
        assert(curr->m_next[0] != nullptr);
        new_node->m_prev = curr;
        new_node->m_next[0]->m_prev = new_node;
        ++m_size;

        return new_node;
    }

    void remove(node_type* node)
    {
        assert(nullptr != node);
        assert(nullptr != node->m_next[0]);
        assert(m_head != node);
        assert(m_tail != node);

        node->m_next[0]->m_prev = node->m_prev;;

        node_type* curr = m_head;
        for (level_type level = m_levels + 1; level > 0;) {
            --level;
            assert(level <= curr->m_level);
            while (curr->m_next[level] != m_tail && m_less(curr->m_next[level]->m_value, node->m_value)) {
                curr = curr->m_next[level];
            }
            if (curr->m_next[level] == node)
            {
                curr->m_next[level] = node->m_next[level];
            }
        }

        delete node;
        --m_size;
    }

    void remove_all()
    {
        node_type* node = m_head->m_next[0];
        while (node != m_tail) {
            node_type* next = node->m_next[0];
            delete node;
            node = next;
        }
        for (level_type level = 0; level < m_levels + 1; ++level) {
            m_head->m_next[level] = m_tail;
        }
        m_tail->m_prev = m_head;
        m_size = 0;
    }

    void dump() const
    {
        for (level_type level = 0; level <= m_levels; ++level) {
            std::cout << "L" << level << ": " << std::flush;
            node_type* curr = m_head->m_next[level];
            std::cout << m_head->m_value << " -> "<< std::flush;
            while (curr != m_tail) {
                std::cout << curr->m_value << "(" << curr->m_prev->m_value << ") -> "<< std::flush;
                curr = curr->m_next[level];
            }
            std::cout << m_tail->m_value << "(" << m_tail->m_prev->m_value << ")" << std::endl;
        }
    }

public:
    bool is_less(const_reference lhs, const_reference rhs) const
    {
        return m_less(lhs, rhs);
    }

    bool is_great(const_reference lhs, const_reference rhs) const
    {
        return m_less(rhs, lhs);
    }

    bool is_less_or_equal(const_reference lhs, const_reference rhs) const
    {
        return !m_less(rhs, lhs);
    }

    bool is_great_or_equal(const_reference lhs, const_reference rhs) const
    {
        return !m_less(lhs, rhs);
    }

    bool is_equal(const_reference lhs, const_reference rhs) const
    {
        return !(m_less(lhs, rhs) || m_less(rhs, lhs));
    }

private:
    allocator_type m_alloc;
    level_type m_levels;
    size_type m_size;
    node_type* m_head;
    node_type* m_tail;
    compare m_less;

}; // sl_impl

template <typename SkipList>
class sl_iterator
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = typename SkipList::value_type;
    // using difference_type = size_t;
    using pointer = typename SkipList::pointer;
    using const_pointer = typename SkipList::const_pointer;
    using reference = typename SkipList::reference;
    using const_reference = typename SkipList::const_reference;

private:
    using node_type = typename SkipList::node_type;
    using self_type = sl_iterator<SkipList>;

public:
    explicit sl_iterator(node_type* node)
        : m_node(node)
    { }

    self_type& operator++()
    {
        m_node = m_node->m_next[0];
        return *this;
    }
    self_type operator++(int)
    {
        self_type tmp(*this);
        m_node = m_node->m_next[0];
        return tmp;
    }

    self_type &operator--()
    {
        m_node = m_node->m_prev;
        return *this;
    }
    self_type operator--(int)
    {
        self_type tmp(*this);
        m_node = m_node->m_prev;
        return tmp;
    }

    const_reference operator*()  { return m_node->m_value; }
    const_pointer   operator->() { return m_node->m_value; }

    bool operator==(const self_type& other) const
    {
        return m_node == other.m_node;
    }
    bool operator!=(const self_type& other) const
    {
        return !operator==(other);
    }

    /**
     * @brief Get pointer to the current node
     * @internal
     */
    node_type* get_node() const
    {
        return m_node;
    }

private:
    node_type* m_node;

}; // sl_iterator


template <typename SkipList>
class sl_const_iterator
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = typename SkipList::value_type;
    // using difference_type = size_t;
    using pointer = typename SkipList::pointer;
    using const_pointer = typename SkipList::const_pointer;
    using reference = typename SkipList::reference;
    using const_reference = typename SkipList::const_reference;

private:
    using node_type = typename SkipList::node_type;
    using self_type = sl_const_iterator<SkipList>;

public:
    explicit sl_const_iterator(node_type* node)
        : m_node(node)
    { }

    sl_const_iterator(sl_iterator<SkipList>& it)
        : m_node(it.get_node())
    { }

    self_type& operator++()
    {
        m_node = m_node->m_next[0];
        return *this;
    }
    self_type operator++(int)
    {
        self_type tmp(*this);
        m_node = m_node->m_next[0];
        return tmp;
    }

    self_type &operator--()
    {
        m_node = m_node->m_prev;
        return *this;
    }
    self_type operator--(int)
    {
        self_type tmp(*this);
        m_node = m_node->m_prev;
        return tmp;
    }

    const_reference operator*()  { return m_node->m_value; }
    const_pointer   operator->() { return m_node->m_value; }

    bool operator==(const self_type& other) const
    {
        return m_node == other.m_node;
    }
    bool operator!=(const self_type& other) const
    {
        return !operator==(other);
    }

    /**
     * @brief Get pointer to the current node
     * @internal
     */
    node_type* get_node() const
    {
        return m_node;
    }

private:
    node_type* m_node;

}; // sl_const_iterator

} // namespace internal

} // namespace skip_list
