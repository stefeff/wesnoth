#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <utility>

namespace utils
{

class two_power_allocator
{
public:

    two_power_allocator();
    ~two_power_allocator();

    std::pair<void *, std::size_t> allocate(std::size_t size);
    void release(void* ptr, std::size_t size);

private:

    struct header
    {
        struct header* next;
    };

    std::array<header*, 64> headers_{};
};

class arena
{
public:

    void* allocate(std::size_t len)
    {
        assert((memory_ == nullptr) == (head_ == nullptr));
        assert(users_);
        len = (len + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
        if (available_ < len) [[unlikely]] {
            slow_allocate(len);
        }

        char* result = memory_;
        memory_ += len;
        available_ -= len;
        return result;
    }

private:

    static constexpr std::size_t MIN_BLOCK_SIZE = 0x1000;
    static constexpr std::size_t MAX_BLOCK_SIZE = 0x100000;

    arena();
    ~arena();

    static two_power_allocator& default_allocator();
    void slow_allocate(std::size_t len);

    struct header {
        std::size_t len;
        struct header* next;
    };

    two_power_allocator& allocator_;
    std::size_t users_;
    char* memory_;
    std::size_t available_;
    header* head_;

    friend class arena_pointer;
};

class arena_pointer
{
public:

    explicit arena_pointer() : arena_{ new arena() } { inc(); }
    explicit arena_pointer(arena* a) noexcept : arena_{a} { inc(); }
    arena_pointer(const arena_pointer& rhs) noexcept : arena_{rhs.arena_} { inc(); }
    arena_pointer(arena_pointer&& rhs) noexcept : arena_{rhs.arena_} { rhs.arena_ = nullptr; }
    ~arena_pointer() noexcept { dec(); }

    arena_pointer& operator=(const arena_pointer& rhs) { rhs.inc(); dec(); arena_ = rhs.arena_; return *this; }
    arena_pointer& operator=(arena_pointer&& rhs) { std::swap(arena_, rhs.arena_); return *this; }

    arena* operator->() const { return arena_; };
    bool operator==(const arena_pointer& rhs) const { return arena_ == rhs.arena_; }
    bool operator!=(const arena_pointer& rhs) const { return !operator==(rhs); }

    void swap(arena_pointer& rhs) { std::swap(arena_, rhs.arena_); }

private:

    void inc() const { if (arena_) ++arena_->users_; }
    void dec() const { if (arena_ && --arena_->users_ == 0) delete arena_; }

    arena* arena_;
};

template<typename T>
class arena_allocator
{
public:

    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef std::true_type propagate_on_container_copy_assignment;
    typedef std::true_type propagate_on_container_move_assignment;
    typedef std::true_type propagate_on_container_swap;

    arena_allocator(const arena_pointer& a) noexcept : arena_{a} {}
    arena_allocator(const arena_allocator& rhs) noexcept : arena_{rhs.arena_} {}
    template<class U>
    arena_allocator(const arena_allocator<U>& rhs) noexcept : arena_{rhs.arena_} {}

    [[nodiscard]] constexpr T* allocate(std::size_t n)
    {
        return reinterpret_cast<T*>(arena_->allocate(n * sizeof(T)));
    }
    [[nodiscard]] constexpr T* allocate(std::size_t n, const void*)
    {
        return reinterpret_cast<T*>(arena_->allocate(n * sizeof(T)));
    }
    constexpr void deallocate(T*, std::size_t) noexcept {}

    template<typename T1, typename T2>
    friend bool operator==(const arena_allocator<T1>& lhs, const arena_allocator<T2>& rhs) noexcept;
    template<typename T1, typename T2>
    friend bool operator!=(const arena_allocator<T1>& lhs, const arena_allocator<T2>& rhs) noexcept;

private:

    arena_pointer arena_;

    template<class O>
    friend class arena_allocator;
};


template<typename T1, typename T2>
inline bool operator==(const arena_allocator<T1>& lhs, const arena_allocator<T2>& rhs) noexcept
{
    return &lhs.arena_ == &rhs.arena_;
}

template<typename T1, typename T2>
inline bool operator!=(const arena_allocator<T1>& lhs, const arena_allocator<T2>& rhs) noexcept
{
    return !operator==(lhs, rhs);
}

}

namespace std
{
template<>
inline void swap( utils::arena_pointer& a, utils::arena_pointer& b )
{
    a.swap(b);
}

}
