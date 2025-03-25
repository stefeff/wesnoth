#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace utils {

class interned_string
{
public:

    interned_string() = default;
    interned_string(const char* s) { init(s); }
    interned_string(const std::string& s) { init(s); }

    bool operator==(const interned_string& rhs) const { return index_ == rhs.index_; }
    bool operator!=(const interned_string& rhs) const { return !operator==(rhs); }

    const std::string& str() const { return shared_->storage[index_]; }
    operator const std::string&() const { return str(); }
    const char* c_str() const { return str().c_str(); }

    const char& operator[](std::size_t index) const { return str()[index]; }

    std::size_t size() const { return str().size(); }
    std::size_t length() const { return size(); }

    std::string_view substr(std::size_t pos = 0, std::size_t count = std::string::npos) const;
    int compare(std::size_t pos, std::size_t count, const char* s) const { return str().compare(pos, count, s); }

    std::string::const_iterator begin() const { return str().begin(); }
    std::string::const_iterator end() const { return str().end(); }

    bool empty() const { return index_ == 0; }

private:

    struct shared
    {
        // empty string is always at index 0
        std::unordered_map<std::string, std::size_t> dictionary = { {"", 0}};
        std::vector<std::string> storage = {""};
    };

    static shared& get_shared() {
        static shared container;
        return container;
    }

    void init(const std::string& s);

    static shared* shared_;
    std::uint32_t index_{0};

    friend class std::hash<interned_string>;
};

}

inline std::ostream& operator<<(std::ostream& os, const utils::interned_string& s)
{
    return os << s.str();
}

inline std::string operator+(const char* lhs, const utils::interned_string& rhs)
{
    return lhs + rhs.str();
}

inline bool operator==(const std::string& lhs, const utils::interned_string& rhs)
{
    return lhs == rhs.str();
}

namespace std
{

template <> struct hash<utils::interned_string>
{
    size_t operator()(const utils::interned_string& s) const
    {
        return s.index_;
    }
};

}
