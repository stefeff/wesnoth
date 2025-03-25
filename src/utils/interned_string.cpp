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
    auto& shared = get_shared();
    if (!shared_) {
        shared_ = &shared;
    }

    auto& index = shared.dictionary[s];
    if (index == 0 && s.size()) {
        index = shared.storage.size();
        shared.storage.push_back(s);
    }

    index_ = index;
}

}