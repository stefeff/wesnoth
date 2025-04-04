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
        return { new char[to_alloc]{}, to_alloc };
    }
}

void two_power_allocator::release(void* ptr, std::size_t size)
{
    auto index = __builtin_clzll(size - 1);
    header* p = reinterpret_cast<header*>(ptr);
    p->next = headers_[index];
    headers_[index] = p;
}

arena::arena()
    : allocator_{default_allocator()}
    , users_{0}
    , memory_{nullptr}
    , available_{0}
    , head_{nullptr}
{
}

arena::~arena()
{
    while (head_) {
        auto next = head_->next;
        allocator_.release(head_, head_->len);
        head_ = next;
    }
}

two_power_allocator& arena::default_allocator()
{
    static two_power_allocator allocator;
    return allocator;
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

}
