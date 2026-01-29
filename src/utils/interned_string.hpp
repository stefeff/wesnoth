#pragma once

#include "tstring.hpp"

#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "quick_hash.hpp"

namespace utils {

class interned_string
{
public:

    interned_string() = default;
    interned_string(const char* s) { init(std::string_view{s}); }
    explicit interned_string(const std::string_view& s) { init(s); }
    interned_string(const std::string& s) { init(s); }
    interned_string(const t_string& s) { init(s.str()); }
    interned_string(const char* s, std::size_t len) { init(std::string_view{s, len}); }

    bool operator==(const interned_string& rhs) const { return index_ == rhs.index_; }
    bool operator!=(const interned_string& rhs) const { return !operator==(rhs); }
    bool operator<(const interned_string& rhs) const { return str() < rhs.str(); }

    const std::string& str() const { return shared_->storage[index_]; }
    operator const std::string&() const { return str(); }
    operator std::string_view() const { return str(); }
    const char* c_str() const { return str().c_str(); }
    const char* data() const { return str().data(); }

    const char& operator[](std::size_t index) const { return str()[index]; }

    std::size_t size() const { return str().size(); }
    std::size_t length() const { return size(); }

    std::string_view substr(std::size_t pos = 0, std::size_t count = std::string::npos) const;
    std::size_t find(char c, std::size_t pos = 0) const { return str().find(c, pos); }
    bool starts_with(const interned_string& s) const { return substr(0, s.size()) == s; }

    int compare(std::size_t pos, std::size_t count, const char* s) const { return str().compare(pos, count, s); }

    std::string::const_iterator begin() const { return str().begin(); }
    std::string::const_iterator end() const { return str().end(); }

    bool empty() const { return index_ == 0; }

private:

    struct hash_func
    {
        std::size_t operator()(const std::string& s) const {
            return f(s.data(), s.size());
        }
        std::size_t operator()(const std::string_view& s) const {
            return f(s.data(), s.size());
        }

    private:

        static std::size_t f(const char* s, std::size_t len) {
            if (len >= 8) [[likely]] {
                std::uint64_t first;
                std::memcpy(&first, s, sizeof(first));
                std::uint64_t last;
                std::memcpy(&last, s + len - sizeof(last), sizeof(last));
                return first ^ (last << 5);
            }
            else if (len >= 4) [[likely]] {
                std::uint32_t first;
                std::memcpy(&first, s, sizeof(first));
                std::uint32_t last;
                std::memcpy(&last, s + len - sizeof(last), sizeof(last));
                return first | static_cast<std::size_t>(last) << 32;
            }
            else if (len == 3) {
                return s[0] + (s[1] << 8) + (s[2] << 16);
            }
            else if (len == 2) {
                return s[0] + (s[1] << 8);
            }
            else if (len == 1) {
                return s[0];
            }
            else {
                return 0;
            }
        }
    };

    struct equal_func
    {
        bool operator()(const std::string& lhs, const std::string& rhs) const {
            return (lhs.size() == rhs.size()) && f(lhs.data(), rhs.data(), lhs.size());
        }
        bool operator()(const std::string_view& lhs, const std::string_view& rhs) const {
            return (lhs.size() == rhs.size()) && f(lhs.data(), rhs.data(), lhs.size());
        }
        bool operator()(const std::string& lhs, const std::string_view& rhs) const {
            return (lhs.size() == rhs.size()) && f(lhs.data(), rhs.data(), lhs.size());
        }
        bool operator()(const std::string_view& lhs, const std::string& rhs) const {
            return (lhs.size() == rhs.size()) && f(lhs.data(), rhs.data(), lhs.size());
        }

    private:

        template<typename T>
        static bool is_equal(const char* lhs, const char* rhs) {
            T l;
            std::memcpy(&l, lhs, sizeof(l));
            T r;
            std::memcpy(&r, rhs, sizeof(r));
            return l == r;
        }

        static bool f(const char* lhs, const char* rhs, std::size_t len) {
            if (len > 8) {
                if (len <= 16) {
                    return is_equal<std::uint64_t>(lhs, rhs)
                        && is_equal<std::uint64_t>(lhs + len - 8, rhs + len - 8);
                }
                else {
                    while (len > 16) {
                        if (!is_equal<std::uint64_t>(lhs, rhs)) {
                            return false;
                        }
                        else {
                            lhs += 8;
                            rhs += 8;
                            len -= 8;
                        }
                    }
                    return is_equal<std::uint64_t>(lhs, rhs)
                        && is_equal<std::uint64_t>(lhs + len - 8, rhs + len - 8);
                }
            }
            else if (len >= 4) {
                return is_equal<std::uint32_t>(lhs, rhs)
                    && is_equal<std::uint32_t>(lhs + len - 4, rhs + len - 4);
            }
            else if (len == 0) {
                return true;
            }
            else {
                return (lhs[0] == rhs[0])
                    && (lhs[len-1] == rhs[len-1])
                    && (lhs[len/2] == rhs[len/2]);
            }
        }
    };

    struct shared
    {
        // empty string is always at index 0
        utils::hash_map<std::string, std::size_t, hash_func, equal_func> dictionary = { {"", 0}};
        std::vector<std::string> storage = {""};
    };

    static shared& get_shared() {
        static shared container;
        return container;
    }

    void init(const std::string& s);
    void init(const std::string_view& s);

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
