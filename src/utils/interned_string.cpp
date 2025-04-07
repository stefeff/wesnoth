#include "interned_string.hpp"

namespace utils {

interned_string::shared* interned_string::shared_{nullptr};

std::string_view interned_string::substr(std::size_t pos, std::size_t count) const
{
    auto& s = str();
    pos = std::min(s.size(), pos);
    return { s.c_str() + pos, std::min(s.size() - pos, count) };
}

void interned_string::init(const std::string& s)
{
    if (!shared_) {
        shared_ = &get_shared();
    }

    if (s.size()) {
        index_ = shared_->dictionary.try_find(s, index_);
        if (index_ == 0) {
            index_ = shared_->storage.size();
            shared_->storage.push_back(s);
            shared_->dictionary[s] = index_;
        }
    }
}

}