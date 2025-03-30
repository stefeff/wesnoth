#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

namespace utils
{

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
class hash_base
{
public:

    using value_type = T;
    using pointer = value_type*;
    using reference = value_type&;

    class const_iterator;

    class iterator
    {
    public:
        using value_type = ConstKeyT;
        using iterator_category = std::bidirectional_iterator_tag;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = std::size_t;

		reference operator*() const { return data_[index_].template as<value_type>(); }
		pointer operator->() const { return &data_[index_].template as<value_type>(); }

        bool operator==(const iterator& rhs) const {
            assert(data_ == rhs.data_);
            return index_ == rhs.index_;
        }
        bool operator!=(const iterator& rhs) const { return !operator==(rhs); }
        bool operator==(const const_iterator& rhs) const {
            assert(data_ == rhs.data_);
            return index_ == rhs.index_;
        }
        bool operator!=(const const_iterator& rhs) const { return !operator==(rhs); }

        iterator& operator++() { index_ = data_[index_].next; return *this; }
        iterator operator++(int) { return { data_, data_[index_].next }; }
        iterator& operator--() { index_ = data_[index_].prior; return *this; }
        iterator operator--(int) { return { data_, data_[index_].prior }; }

    private:

        iterator(typename hash_base::entry_t* data, typename hash_base::index_t index)
            : data_{data}, index_{index} {}

        typename hash_base::entry_t* data_;
        typename hash_base::index_t index_;

        friend class hash_base;
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
            : data_{rhs.data_}, index_{rhs.index_} {}

		reference operator*() const { return data_[index_].template as<value_type>(); }
		pointer operator->() const { return &data_[index_].template as<value_type>(); }

        bool operator==(const const_iterator& rhs) const {
            assert(data_ == rhs.data_);
            return index_ == rhs.index_;
        }
        bool operator!=(const const_iterator& rhs) const { return !operator==(rhs); }
        bool operator==(const iterator& rhs) const {
            assert(data_ == rhs.data_);
            return index_ == rhs.index_;
        }
        bool operator!=(const iterator& rhs) const { return !operator==(rhs); }

        const_iterator& operator++() { index_ = data_[index_].next; return *this; }
        const_iterator operator++(int) { return { data_, data_[index_].next }; }
        const_iterator& operator--() { index_ = data_[index_].prior; return *this; }
        const_iterator operator--(int) { return { data_, data_[index_].prior }; }

    private:

        const_iterator(const typename hash_base::entry_t* data, typename hash_base::index_t index)
            : data_{data}, index_{index} {}

        const typename hash_base::entry_t* data_;
        typename hash_base::index_t index_;

        friend class hash_base;
        friend class iterator;
    };

    hash_base() { index_.resize(16); }
    hash_base(const hash_base& rhs);
    hash_base(hash_base&& rhs);
    template<class I>
    hash_base(I first, I last) { index_.resize(16); insert(first, last); }
    hash_base(std::initializer_list<value_type> init) { index_.resize(16); insert(init.begin(), init.end()); }
    ~hash_base() { deconstruct_all(); }

    hash_base& operator=(const hash_base& rhs);
    hash_base& operator=(hash_base&& rhs);
    void swap(hash_base& rhs);

    bool operator==(const hash_base& rhs) const;
    bool operator!=(const hash_base& rhs) const { return !operator==(rhs); }

    std::size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }

    iterator begin() { return { data_.data(), first_ }; }
    const_iterator begin() const { return { data_.data(), first_ }; }
    const_iterator cbegin() const { return { data_.data(), first_ }; }
    iterator end() { return { data_.data(), NO_INDEX }; }
    const_iterator end() const { return { data_.data(), NO_INDEX }; }
    const_iterator cend() const { return { data_.data(), NO_INDEX }; }

    bool contains(const Key& key) const { return internal_find(key).found; }
    std::size_t count(const Key& key) const { return internal_find(key).found ? 1 : 0; }
    iterator find(const Key& key) { auto pos = internal_find(key); return { data_.data(), pos.found ? pos.data_index : NO_INDEX }; }
    const_iterator find(const Key& key) const { auto pos = internal_find(key); return { data_.data(), pos.found ? pos.data_index : NO_INDEX }; }

    std::pair<iterator, bool> insert(const T& item);
    template< class... Args >
    std::pair<iterator, bool> emplace( Args&&... args );
    iterator insert(iterator, const T& item) { return insert(item).first; }
    template<class I>
    void insert(I first, I last);
    iterator erase(iterator pos);
    std::size_t erase(const Key& key);
    void clear();

private:

    using index_t = std::uint32_t;
    static constexpr index_t NO_INDEX = 0;

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

        index_t prior    : 31;
        index_t is_start : 1;
        index_t next     : 31;
        index_t is_end   : 1;
        index_t hash_index;
    };

    struct lookup_result
    {
        index_t hash_index;
        index_t data_index;
        bool found;
    };

    lookup_result internal_find(const Key& key) const;
    void internal_erase(index_t index);
    void grow_data();
    void grow_data(std::size_t new_size);
    void rehash();
    void link(index_t index, index_t prior_in_bucket);
    void deconstruct_all();
    void verify();

    std::vector<index_t> index_;
    std::vector<entry_t> data_;
    std::size_t count_{0};

    index_t first_{NO_INDEX};
    index_t last_{NO_INDEX};
    index_t first_unused_{NO_INDEX};

    KeyAccess key_access_;
    Hash hash_;

    friend class iterator;
    friend class const_iterator;
};

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::hash_base(const hash_base& rhs)
{
    if (!rhs.empty()) {
        index_.resize(std::min(rhs.index_.size(), 2 * rhs.size()));
        grow_data(std::min(rhs.data_.size(), 2 * rhs.size()));
        insert(rhs.begin(), rhs.end());
    }
    else {
        index_.resize(16);
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::hash_base(hash_base&& rhs)
    : index_{std::move(rhs.index_)}
    , data_{std::move(rhs.data_)}
    , count_{rhs.count_}
    , first_{rhs.first_}
    , last_{rhs.last_}
    , first_unused_{rhs.first_unused_}
{
    rhs.count_ = 0;
    rhs.first_ = NO_INDEX;
    rhs.last_ = NO_INDEX;
    rhs.first_unused_ = NO_INDEX;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
auto hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::operator=(const hash_base& rhs) -> hash_base&
{
    if (this != &rhs && (!empty() || !rhs.empty())) {
        clear();
        insert(rhs.begin(), rhs.end());
    }

    return *this;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
auto hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::operator=(hash_base&& rhs) -> hash_base&
{
    deconstruct_all();

    index_.swap(rhs.index_);
    data_.swap(rhs.data_);

    count_ = rhs.count_;
    first_ = rhs.first_;
    last_ = rhs.last_;
    first_unused_ = rhs.first_unused_;
    key_access_ = rhs.key_access_;
    hash_ = rhs.hash_;

    rhs.count_ = 0;
    rhs.first_ = NO_INDEX;
    rhs.last_ = NO_INDEX;
    rhs.first_unused_ = NO_INDEX;

    return *this;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::swap(hash_base& rhs)
{
    index_.swap(rhs.index_);
    data_.swap(rhs.data_);
    std::swap(count_, rhs.count_);

    std::swap(first_, rhs.first_);
    std::swap(last_, rhs.last_);
    std::swap(first_unused_, rhs.first_unused_);

    std::swap(key_access_, rhs.key_access_);
    std::swap(hash_, rhs.hash_);
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
bool hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::operator==(const hash_base& rhs) const
{
    if (count_ != rhs.count_) {
        return false;
    }

    for (auto& item : *this) {
        if (!rhs.contains(item)) {
            return false;
        }
    }

    return true;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
auto hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::insert(const T& item) -> std::pair<iterator, bool>
{
    auto& key = key_access_(item);
    auto pos = internal_find(key);
    if (pos.found) {
        return { { data_.data(), pos.data_index }, false };
    }
    else {
        if (first_unused_ == NO_INDEX) {
            grow_data();
        }

        auto index = first_unused_;
        auto& entry = data_[index];
        first_unused_ = entry.next;
        new (&entry.data) T(item);
        ++count_;

        entry.hash_index = pos.hash_index;
        link(index, pos.data_index);

        if (!entry.is_start && 2 * count_ > index_.size()) {
            rehash();
            pos = internal_find(key);

            return { { data_.data(), pos.data_index }, true };
        }

        return { { data_.data(), index }, true };
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
template< class... Args >
auto hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::emplace( Args&&... args ) -> std::pair<iterator, bool>
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
        return { { data_.data(), pos.data_index }, false };
    }
    else {
        auto index = first_unused_;
        first_unused_ = entry.next;
        ++count_;

        entry.hash_index = pos.hash_index;
        link(index, pos.data_index);

        if (2 * count_ > index_.size()) {
            auto pos = internal_find(key);
            return { { data_.data(), pos.data_index }, true };
        }

        return { { data_.data(), index }, true };
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
template<class I>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::insert(I first, I last)
{
    for (; first != last; ++first) {
        insert(*first);
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
auto hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::erase(iterator pos) -> iterator
{
    assert(pos.data_ == data_.data());
    auto index = pos.index_;
    ++pos;
    internal_erase(index);
    return pos;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
std::size_t hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::erase(const Key& key)
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

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::clear()
{
    for (auto index = first_; index != NO_INDEX; ) {
        auto& entry = data_[index];
        index_[entry.hash_index] = NO_INDEX;
        auto next = entry.next;

        entry.template as<T>().~T();
        entry.next = first_unused_;
        first_unused_ = index;
        index = next;
    }

    first_ = NO_INDEX;
    last_ = NO_INDEX;
    count_ = 0;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
auto hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::internal_find(const Key& key) const -> lookup_result
{
    lookup_result result;
    result.hash_index = hash_(key) % index_.size();
    result.data_index = index_[result.hash_index];

    if (result.data_index) {
        while (true) {
            auto& entry = data_[result.data_index];
            if (key_access_(entry.template as<T>()) == key) {
                result.found = true;
                return result;
            }
            if (entry.is_end) {
                break;
            }
            else {
                result.data_index = entry.next;
            }
        }
    }

    result.found = false;
    return result;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::internal_erase(index_t index)
{
    auto& entry = data_[index];

    if (entry.is_start) {
        if (entry.is_end) {
            index_[entry.hash_index] = NO_INDEX;
        }
        else {
            index_[entry.hash_index] = entry.next;
        }
    }

    if (entry.prior == NO_INDEX) {
        first_ = entry.next;
    }
    else {
        data_[entry.prior].next = entry.next;
        data_[entry.prior].is_end |= entry.is_end;
    }

    if (entry.next == NO_INDEX) {
        last_ = entry.prior;
    }
    else {
        data_[entry.next].prior = entry.prior;
        data_[entry.next].is_start |= entry.is_start;
    }

    entry.template as<T>().~T();
    entry.next = first_unused_;
    first_unused_ = index;

    --count_;
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::grow_data()
{
    auto old_size = data_.size();
    grow_data(old_size ? 2 * old_size : 16);
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::grow_data(std::size_t new_size)
{
    assert(first_unused_ == NO_INDEX);

    auto old_size = data_.size();
    data_.resize(new_size);
    first_unused_ = static_cast<index_t>(old_size ? old_size : 1);
    for (auto i = first_unused_ + 1; i < new_size; ++i) {
        data_[i - 1].next = i;
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::rehash()
{
    auto new_size = (4 << (32 - __builtin_clzl(count_ + 1))) - 1;
    index_.clear();
    index_.resize(new_size);

    index_t index = first_;
    first_ = NO_INDEX;
    last_ = NO_INDEX;
    while (index != NO_INDEX) {
        auto& entry = data_[index];
        auto next = entry.next;
        entry.hash_index = hash_(key_access_(entry.template as<T>())) % index_.size();
        link(index, index_[entry.hash_index]);
        index = next;
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::link(index_t index, index_t prior_in_bucket)
{
    auto& entry = data_[index];
    if (prior_in_bucket == NO_INDEX) {
        entry.prior = last_;
        entry.is_start = true;
        entry.next = NO_INDEX;
        entry.is_end = true;
        index_[entry.hash_index] = index;

        if (first_ == NO_INDEX) {
            first_ = index;
        }
        else {
            data_[last_].next = index;
        }
        last_ = index;
    }
    else {
        auto& prior = data_[prior_in_bucket];

        entry.prior = prior_in_bucket;
        entry.is_start = false;
        if (prior_in_bucket == last_) {
            entry.next = NO_INDEX;
            entry.is_end = true;
            last_ = index;
        }
        else {
            entry.next = prior.next;
            entry.is_end = prior.is_end;
            data_[entry.next].prior = index;
        }
        prior.next = index;
        prior.is_end = false;
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::deconstruct_all()
{
    for (auto index = first_; index != NO_INDEX; ) {
        auto& entry = data_[index];
        entry.template as<T>().~T();
        index = entry.next;
    }
}

template <class T, class Key, class ConstKeyT, class KeyAccess, class Hash>
void hash_base<T, Key, ConstKeyT, KeyAccess, Hash>::verify()
{
    for (auto index = first_; index != NO_INDEX; ) {
        auto& entry = data_[index];
        assert(entry.prior < data_.size());
        assert(entry.next < data_.size());
        assert(entry.hash_index < index_.size());

        if (entry.is_start) {
            if (entry.prior != NO_INDEX) {
                auto& prior = data_[entry.prior];
                assert(prior.is_end && prior.hash_index != entry.hash_index);
            }
            assert(index_[entry.hash_index] == index);
        }
        else {
            assert(index_[entry.hash_index] != index);
            assert(entry.prior != NO_INDEX);

            auto& prior = data_[entry.prior];
            assert(!prior.is_end && prior.hash_index == entry.hash_index);
        }

        assert((last_ == index) == (entry.next == NO_INDEX));
        index = entry.next;
    }
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

template <class T, class Hash = std::hash<T> >
using hash_set = hash_base<T, T, const T, identity<T>, Hash>;

template <class K, class V, class Hash = std::hash<K> >
using hash_map = hash_base<std::pair<K, V>, K, std::pair<const K, V>, get_first<K,V>, Hash>;

}
