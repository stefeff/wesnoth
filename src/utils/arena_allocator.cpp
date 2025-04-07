#include "arena_allocator.hpp"

#include <cstring>

namespace utils
{

two_power_allocator::two_power_allocator()
{
}

two_power_allocator::~two_power_allocator()
{
    for (auto bucket : headers_) {
        while (bucket) {
            auto next = bucket->next;
            delete[] reinterpret_cast<char*>(bucket);
            bucket = next;
        }
    }

    while (unused_) {
        auto arena = unused_;
        unused_ = arena->next_;
        delete arena;
    }
}

std::pair<void *, std::size_t> two_power_allocator::allocate(std::size_t size)
{
    auto index = __builtin_clzll(size - 1);
    std::size_t to_alloc = 1ull << (64 - index);
    auto head = headers_[index];
    if (head) {
        headers_[index] = head->next;
        return { head, to_alloc };
    }
    else {
        return { new char[to_alloc], to_alloc };
    }
}

void two_power_allocator::release(void* ptr, std::size_t size)
{
#if 1
    auto index = __builtin_clzll(size - 1);
    // memset(ptr, 211, size);
    header* p = reinterpret_cast<header*>(ptr);
    p->next = headers_[index];
    headers_[index] = p;
#else
    memset(ptr, 0, size);
    delete[] reinterpret_cast<char*>(ptr);
#endif
}

arena* two_power_allocator::acquire_arena()
{
    if (unused_) {
        arena* result = unused_;
        unused_ = result->next_;
        return result;
    }
    else {
        return new arena(*this);
    }
}

void two_power_allocator::release_arena(arena* a)
{
    a->next_ = unused_;
    unused_ = a;
}

arena* arena::acquire()
{
    return two_power_allocator::default_allocator().acquire_arena();
}

void arena::release()
{
    clear();
    allocator_.release_arena(this);
}

arena::arena(two_power_allocator& allocator)
    : allocator_{allocator}
    , users_{0}
    , memory_{nullptr}
    , available_{0}
    , head_{nullptr}
    , next_{nullptr}
{
}

void arena::slow_allocate(std::size_t len)
{
    len += sizeof(header);
    if (len < MAX_BLOCK_SIZE) {
        if (head_) {
            len = std::min(2 * std::max(len, head_->len), MAX_BLOCK_SIZE);
        }
        else {
            len = std::max(len, MIN_BLOCK_SIZE);
        }
    }

    auto block = allocator_.allocate(len);
    header* new_header = reinterpret_cast<header*>(block.first);
    new_header->len = block.second;
    new_header->next = head_;

    memory_ = reinterpret_cast<char*>(block.first) + sizeof(header);
    available_ = block.second - sizeof(header);
    head_ = new_header;
}

void arena::clear()
{
    while (head_) {
        auto next = head_->next;
        allocator_.release(head_, head_->len);
        head_ = next;
    }

    memory_ = nullptr;
    available_ = 0;
}

}
