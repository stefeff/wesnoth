#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

namespace utils
{

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
class hash_container
{
public:

    using value_type = T;
    using key_type = Key;
    using pointer = value_type*;
    using reference = value_type&;

    class const_iterator;

    class iterator
    {
    public:
        using value_type = T;
        using iterator_category = std::bidirectional_iterator_tag;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = std::size_t;

		reference operator*() const { return container_->data_[index_].template as<value_type>(); }
		pointer operator->() const { return &container_->data_[index_].template as<value_type>(); }

        bool operator==(const iterator& rhs) const {
            assert(container_ == rhs.container_);
            return index_ == rhs.index_;
        }
        bool operator!=(const iterator& rhs) const { return !operator==(rhs); }
        bool operator==(const const_iterator& rhs) const {
            assert(container_ == rhs.container_);
            return index_ == rhs.index_;
        }
        bool operator!=(const const_iterator& rhs) const { return !operator==(rhs); }

        iterator& operator++() { forward(); return *this; }
        iterator operator++(int) { iterator result{*this}; forward(); return result; }
        iterator& operator--() { backward(); return *this; }
        iterator operator--(int) { iterator result{*this}; backward(); return result;  }

    private:

        iterator(hash_container* container, std::size_t index)
            : container_{container}, index_{index} {}

        void forward() {
            auto current = container_->data_.begin() + index_;
            auto end = container_->data_.end();
            if (current != end) {
                while (++index_, ++current != end) {
                    if (current->hash_index != hash_container::NO_HASH) {
                        break;
                    }
                }
            }
        }

        void backward() {
            while (index_ > 0) {
                if (container_->data_[--index_].hash_index != hash_container::NO_HASH) {
                    break;
                }
            }
        }

        hash_container* container_;
        std::size_t index_;

        friend class hash_container;
        friend class const_iterator;
    };

    class const_iterator
    {
    public:
        using value_type = const T;
        using iterator_category = std::bidirectional_iterator_tag;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = std::size_t;

        const_iterator(iterator rhs)
            : container_{rhs.container_}, index_{rhs.index_} {}

		reference operator*() const { return container_->data_[index_].template as<value_type>(); }
		pointer operator->() const { return &container_->data_[index_].template as<value_type>(); }

        bool operator==(const const_iterator& rhs) const {
            assert(container_ == rhs.container_);
            return index_ == rhs.index_;
        }
        bool operator!=(const const_iterator& rhs) const { return !operator==(rhs); }
        bool operator==(const iterator& rhs) const {
            assert(container_ == rhs.container_);
            return index_ == rhs.index_;
        }
        bool operator!=(const iterator& rhs) const { return !operator==(rhs); }

        const_iterator& operator++() { forward(); return *this; }
        const_iterator operator++(int) { const_iterator result{*this}; forward(); return result; }
        const_iterator& operator--() { backward(); return *this; }
        const_iterator operator--(int) { const_iterator result{*this}; backward(); return result; }

    private:

        const_iterator(const hash_container* container, std::size_t index)
            : container_{container}, index_{index} {}

        void forward() {
            auto current = container_->data_.begin() + index_;
            auto end = container_->data_.end();
            if (current != end) {
                while (++index_, ++current != end) {
                    if (current->hash_index != hash_container::NO_HASH) {
                        break;
                    }
                }
            }
        }

        void backward() {
            while (index_ > 0) {
                if (container_->data_[--index_].hash_index != hash_container::NO_HASH) {
                    break;
                }
            }
        }

        const hash_container* container_;
        std::size_t index_;

        friend class hash_container;
        friend class iterator;
    };

    hash_container(const Allocator& alloc = Allocator());
    hash_container(const hash_container& rhs);
    hash_container(const hash_container& rhs, const Allocator& alloc);
    hash_container(hash_container&& rhs);
    hash_container(hash_container&& rhs, const Allocator& alloc);
    template<class I>
    hash_container(I first, I last) { insert(first, last); }
    hash_container(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }
    ~hash_container() { deconstruct_all(); }

    hash_container& operator=(const hash_container& rhs);
    hash_container& operator=(hash_container&& rhs);
    void swap(hash_container& rhs);

    bool operator==(const hash_container& rhs) const;
    bool operator!=(const hash_container& rhs) const { return !operator==(rhs); }

    std::size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }

    iterator begin() { return ++iterator{ this, 0 }; }
    const_iterator begin() const { return ++const_iterator{ this, 0 }; }
    const_iterator cbegin() const { return ++const_iterator{ this, 0 }; }
    iterator end() { return { this, last_index() }; }
    const_iterator end() const { return { this, last_index() }; }
    const_iterator cend() const { return { this, last_index() }; }

    bool contains(const Key& key) const { return internal_find(key).found; }
    std::size_t count(const Key& key) const { return internal_find(key).found ? 1 : 0; }
    iterator find(const Key& key) {
        auto pos = internal_find(key);
        return { this, pos.found ? pos.data_index : last_index() };
    }
    const_iterator find(const Key& key) const {
        auto pos = internal_find(key);
        return { this, pos.found ? pos.data_index : last_index() };
    }

    std::pair<iterator, bool> insert(const T& item);
    template< class... Args >
    std::pair<iterator, bool> emplace( Args&&... args );
    iterator insert(iterator, const T& item) { return insert(item).first; }
    template<class I>
    void insert(I first, I last);
    iterator erase(iterator pos);
    std::size_t erase(const Key& key);
    void clear();

protected:

    using index_t = std::uint32_t;
    static constexpr index_t NO_INDEX = 0;
    static constexpr index_t NO_HASH = ~0;

    struct entry_t
    {
        char data[sizeof(T[1])];

        template<class Other>
        Other& as() {
            static_assert(sizeof(Other) == sizeof(T));
            return *reinterpret_cast<Other*>(data);
        }
        template<class Other>
        const Other& as() const {
            static_assert(sizeof(Other) == sizeof(T));
            return *reinterpret_cast<const Other*>(data);
        }

        index_t next;
        index_t hash_index;
    };

    struct lookup_result
    {
        index_t hash_index;
        index_t data_index;
        bool found;
    };

    lookup_result internal_find(const Key& key) const;
    template<typename K2>
    const entry_t* optimistic_find(const K2& key) const;
    const entry_t* optimistic_find(const Key& key) const;

    entry_t& insert_key(const Key& key);
    void internal_erase(index_t index);
    void grow_data();
    void grow_data(std::size_t new_size);
    void rehash();
    void link(index_t index);
    void deconstruct_all();
    void verify();
    index_t last_index() const { return static_cast<index_t>(data_.size()); }

    Allocator allocator_;
    std::vector<index_t, typename std::allocator_traits<Allocator>::rebind_alloc<index_t>> index_;
    std::vector<entry_t, typename std::allocator_traits<Allocator>::rebind_alloc<entry_t>> data_;
    std::size_t count_{0};

    index_t first_unused_{NO_INDEX};

    KeyAccess key_access_;
    Hash hash_;
    KeyEqual equal_;

    friend class iterator;
    friend class const_iterator;
};

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::hash_container(const Allocator& alloc)
    : allocator_{alloc}
    , index_{alloc}
    , data_{alloc}
{
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::hash_container(const hash_container& rhs)
    : index_{rhs.index_, allocator_}
    , data_{rhs.data_, allocator_}
    , count_{rhs.count_}
    , first_unused_{rhs.first_unused_}
{
    if (count_) {
        for (size_t i = 1; i < rhs.data_.size(); ++i) {
            auto& source = rhs.data_[i];
            if (source.hash_index != NO_HASH) {
                new (&data_[i].data) T(source.template as<T>());
            }
        }
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::hash_container(const hash_container& rhs, const Allocator& alloc)
    : allocator_{alloc}
    , index_{rhs.index_, allocator_}
    , data_{rhs.data_, allocator_}
    , count_{rhs.count_}
    , first_unused_{rhs.first_unused_}
{
    if (count_) {
        for (size_t i = 1; i < rhs.data_.size(); ++i) {
            auto& source = rhs.data_[i];
            if (source.hash_index != NO_HASH) {
                new (&data_[i].data) T(source.template as<T>());
            }
        }
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::hash_container(hash_container&& rhs)
    : allocator_{std::move(rhs.allocator_)}
    , index_{std::move(rhs.index_)}
    , data_{std::move(rhs.data_)}
    , count_{rhs.count_}
    , first_unused_{rhs.first_unused_}
{
    rhs.count_ = 0;
    rhs.first_unused_ = NO_INDEX;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::hash_container(hash_container&& rhs, const Allocator& alloc)
    : allocator_{alloc}
    , index_{std::move(rhs.index_)}
    , data_{std::move(rhs.data_)}
    , count_{rhs.count_}
    , first_unused_{rhs.first_unused_}
{
    rhs.count_ = 0;
    rhs.first_unused_ = NO_INDEX;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::operator=(const hash_container& rhs) -> hash_container&
{
    if (this != &rhs && (!empty() || !rhs.empty())) {
        clear();
        insert(rhs.begin(), rhs.end());
    }

    return *this;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::operator=(hash_container&& rhs) -> hash_container&
{
    deconstruct_all();

    index_.swap(rhs.index_);
    data_.swap(rhs.data_);

    count_ = rhs.count_;
    first_unused_ = rhs.first_unused_;
    key_access_ = rhs.key_access_;
    hash_ = rhs.hash_;

    rhs.count_ = 0;
    rhs.first_unused_ = NO_INDEX;
    // verify();

    return *this;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::swap(hash_container& rhs)
{
    index_.swap(rhs.index_);
    data_.swap(rhs.data_);

    std::swap(count_, rhs.count_);

    std::swap(first_unused_, rhs.first_unused_);

    std::swap(key_access_, rhs.key_access_);
    std::swap(hash_, rhs.hash_);
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
bool hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::operator==(const hash_container& rhs) const
{
    if (count_ != rhs.count_) {
        return false;
    }

    for (auto& lhs_item : *this) {
        auto rhs_pos = rhs.internal_find(key_access_(lhs_item));
        if (!rhs_pos.found || lhs_item != rhs.data_[rhs_pos.data_index].template as<T>()) {
            return false;
        }
    }

    return true;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::insert(const T& item) -> std::pair<iterator, bool>
{
    auto& key = key_access_(item);
    auto pos = internal_find(key);
    if (pos.found) {
        return { { this, pos.data_index }, false };
    }
    else {
        if (first_unused_ == NO_INDEX) {
            grow_data();
            pos.hash_index = hash_(key) % index_.size();;
        }

        auto index = first_unused_;
        auto& entry = data_[index];
        new (&entry.data) T(item);

        first_unused_ = entry.next;
        ++count_;

        entry.hash_index = pos.hash_index;
        link(index);

        if (entry.next != NO_INDEX && 2 * count_ > index_.size()) {
            rehash();
            pos = internal_find(key);

            return { { this, pos.data_index }, true };
        }

        // verify();
        return { { this, index }, true };
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::insert_key(const Key& key) -> entry_t&
{
    if (first_unused_ == NO_INDEX) {
        grow_data();
    }
    if (2 * count_ > index_.size()) {
        rehash();
    }

    auto index = first_unused_;
    auto& entry = data_[index];
    new (&entry.data) T(std::piecewise_construct_t{}, std::tuple<const Key&>{key}, std::tuple<>{});

    first_unused_ = entry.next;
    ++count_;

    entry.hash_index = hash_(key) % index_.size();
    link(index);
    // verify();

    return entry;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
template< class... Args >
auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::emplace( Args&&... args ) -> std::pair<iterator, bool>
{
    if (first_unused_ == NO_INDEX) {
        grow_data();
    }

    auto& entry = data_[first_unused_];
    new (&entry.data) T(std::forward<Args>(args)...);

    auto& key = key_access_(entry.template as<T>());
    auto pos = internal_find(key);
    if (pos.found) {
        entry.template as<T>().~T();
        return { { this, pos.data_index }, false };
    }
    else {
        auto index = first_unused_;
        first_unused_ = entry.next;
        ++count_;

        entry.hash_index = pos.hash_index;
        link(index);

        if (pos.data_index != NO_INDEX && 2 * count_ > index_.size()) {
            rehash();
            pos = internal_find(key);
            return { { this, pos.data_index }, true };
        }

        return { { this, index }, true };
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
template<class I>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::insert(I first, I last)
{
    for (; first != last; ++first) {
        insert(*first);
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::erase(iterator pos) -> iterator
{
    assert(pos.container_ == this);
    auto index = pos.index_;
    ++pos;
    internal_erase(index);
    return pos;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
std::size_t hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::erase(const Key& key)
{
    auto pos = internal_find(key);
    if (pos.found) {
        internal_erase(pos.data_index);
        return 1;
    }
    else {
        return 0;
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::clear()
{
    if (count_) {
        index_t index = 1;
        for (auto it = data_.begin()+1, end = data_.end(); it != end; ++it, ++index) {
            auto& entry = *it;
            if (entry.hash_index != NO_HASH) {
                index_[entry.hash_index] = NO_INDEX;
                entry.template as<T>().~T();
                entry.hash_index = NO_HASH;
                entry.next = first_unused_;
                first_unused_ = index;
            }
        }
        count_ = 0;
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::internal_find(const Key& key) const -> lookup_result
{
    lookup_result result;
    if (index_.size() > 0) {
        result.hash_index = hash_(key) % index_.size();
        result.data_index = index_[result.hash_index];

        while (result.data_index) {
            auto& entry = data_[result.data_index];
            assert(entry.hash_index == result.hash_index);
            if (equal_(key_access_(entry.template as<T>()), key)) {
                result.found = true;
                return result;
            }
            else {
                result.data_index = entry.next;
            }
        }
    }
    else {
        result.hash_index = NO_INDEX;
        result.data_index = NO_INDEX;
    }

    result.found = false;
    return result;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
template <typename K2>
inline auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::optimistic_find(const K2& key) const -> const entry_t*
{
    auto hash_index = hash_(key) % index_.size();
    auto data_index = index_[hash_index];

    while (data_index) {
        auto& entry = data_[data_index];
        if (equal_(key_access_(entry.template as<T>()), key)) {
            return &entry;
        }
        data_index = entry.next;
    }

    return nullptr;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
inline auto hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::optimistic_find(const Key& key) const -> const entry_t*
{
    auto hash_index = hash_(key) % index_.size();
    auto data_index = index_[hash_index];

    while (data_index) {
        auto& entry = data_[data_index];
        if (equal_(key_access_(entry.template as<T>()), key)) {
            return &entry;
        }
        data_index = entry.next;
    }

    return nullptr;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::internal_erase(index_t index)
{
    auto& entry = data_[index];
    assert(entry.hash_index != NO_HASH);

    if (index_[entry.hash_index] == index) {
        index_[entry.hash_index] = entry.next;
    }
    else {
        auto prior = index_[entry.hash_index];
        while (data_[prior].next != index) {
            prior = data_[prior].next;
        }
        data_[prior].next = entry.next;
    }

    entry.template as<T>().~T();
    entry.hash_index = NO_HASH;
    entry.next = first_unused_;
    first_unused_ = index;

    --count_;
    // verify();
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::grow_data()
{
    auto old_size = data_.size();
    grow_data(old_size ? 2 * old_size : 2);
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::grow_data(std::size_t new_size)
{
    assert(first_unused_ == NO_INDEX);

    // piggy-back: an empty container does not have an index:
    if (index_.empty()) {
        index_.resize(13);
    }

    // verify();
    auto old_size = data_.size();
    decltype(data_) new_data{allocator_};
    new_data.resize(new_size);
    first_unused_ = static_cast<index_t>(old_size ? old_size : 1);

    new_data.front().hash_index = NO_HASH;
    for (index_t i = 0; i < old_size; ++i) {
        auto& source = data_[i];
        auto& dest = new_data[i];

        dest.next = source.next;
        dest.hash_index = source.hash_index;
        if (source.hash_index != NO_HASH) {
            new (&dest.data) T(std::move(source.template as<T>()));
            source.template as<T>().~T();
        }
    }

    new_data.back().hash_index = NO_HASH;
    for (index_t i = first_unused_ + 1; i < new_size; ++i) {
        auto& entry = new_data[i - 1];
        entry.next = i;
        entry.hash_index = NO_HASH;
    }

    data_.swap(new_data);
    // verify();
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::rehash()
{
    // verify();
    auto new_size = (2 << (32 - __builtin_clzl(count_ + 1))) - 1;
    index_.clear();
    index_.resize(new_size);

    for (index_t index = 1; index < data_.size(); ++index) {
        auto& entry = data_[index];
        if (entry.hash_index != NO_HASH) {
            entry.hash_index = hash_(key_access_(entry.template as<T>())) % index_.size();
            link(index);
        }
    }
    // verify();
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
inline void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::link(index_t index)
{
    auto& entry = data_[index];
    entry.next = index_[entry.hash_index];
    index_[entry.hash_index] = index;
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::deconstruct_all()
{
    if (count_) {
        for (auto it = data_.begin()+1, end = data_.end(); it != end; ++it) {
            auto& entry = *it;
            if (entry.hash_index != NO_HASH) [[unlikely]] {
                entry.template as<T>().~T();
            }
        }
    }
}

template <class T, class Key, class KeyAccess, class Hash, class KeyEqual, class Allocator>
void hash_container<T, Key, KeyAccess, Hash, KeyEqual, Allocator>::verify()
{
    std::size_t visited = 0;
    for (index_t hash_index = 0; hash_index < index_.size(); ++hash_index) {
        for (index_t index = index_[hash_index]; index != NO_INDEX; ) {
            auto& entry = data_[index];
            assert(entry.next < data_.size());
            assert(entry.hash_index == hash_index);
            index = entry.next;
            ++visited;

            assert(contains(key_access_(entry.template as<T>())));
        }
    }
    assert(visited == count_);

    for (auto index = first_unused_; index != NO_INDEX; ) {
        auto& entry = data_[index];
        assert(entry.next < data_.size());
        assert(entry.hash_index == NO_HASH);
        index = entry.next;
        ++visited;
    }

    assert(visited + 1 == data_.size() || data_.empty());
}

template<class T>
struct identity
{
    T& operator()(T& v) const { return v; }
    const T& operator()(const T& v) const { return v; }
};

template<class K, class V>
struct get_first
{
    K& operator()(std::pair<K, V>& p) const { return p.first; }
    const K& operator()(const std::pair<K, V>& p) const { return p.first; }
};

template <class T, class Hash = std::hash<T>, class KeyEqual = std::equal_to<T>, class Allocator = std::allocator<T>>
using hash_set = hash_container<T, T, identity<T>, Hash, KeyEqual, Allocator>;

template <class K, class V, class Hash = std::hash<K>, class KeyEqual = std::equal_to<K>, class Allocator = std::allocator<std::pair<const K, V>>>
class hash_map : public hash_container<std::pair<K, V>, K, get_first<K,V>, Hash, KeyEqual, Allocator>
{
private:

    using Base_ = hash_container<std::pair<K, V>, K, get_first<K,V>, Hash, KeyEqual, Allocator>;

public:

    using value_type = typename Base_::value_type;

    hash_map(const Allocator& alloc = Allocator()) : Base_{alloc} { }
    hash_map(const hash_map& rhs) : Base_{rhs} { }
    hash_map(const hash_map& rhs, const Allocator& alloc) : Base_{rhs, alloc} { }
    hash_map(hash_map&& rhs) : Base_{static_cast<Base_&&>(rhs)} { }
    hash_map(hash_map&& rhs, const Allocator& alloc) : Base_{rhs, alloc} { }

    template<class I>
    hash_map(I first, I last, const Allocator& alloc = Allocator())
        : Base_{alloc} { this->insert(first, last); }
    hash_map(std::initializer_list<value_type> init, const Allocator& alloc = Allocator())
        : Base_{alloc} { this->insert(init.begin(), init.end()); }

    hash_map& operator=(const hash_map& rhs) { Base_::operator=(rhs); return *this; }
    hash_map& operator=(hash_map&& rhs) { Base_::operator=(rhs); return *this; }

    V& operator[](const K& key);

    template<typename K2>
    const V& try_find(const K2& key, const V& fallback) const
    {
        auto* entry = Base_::optimistic_find(key);
        return entry ? entry->template as<value_type>().second : fallback;
    }
    const V& try_find(const K& key, const V& fallback) const
    {
        auto* entry = Base_::optimistic_find(key);
        return entry ? entry->template as<value_type>().second : fallback;
    }
};

template <class K, class V, class Hash, class KeyEqual, class Allocator>
V& hash_map<K, V, Hash, KeyEqual, Allocator>::operator[](const K& key)
{
    auto pos = Base_::internal_find(key);
    auto& entry = pos.found ? Base_::data_[pos.data_index] : Base_::insert_key(key);
    return entry.template as<value_type>().second;
}

}
