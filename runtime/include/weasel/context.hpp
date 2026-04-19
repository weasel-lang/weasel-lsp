#pragma once
#include <stdexcept>
#include <vector>

namespace weasel {

template<typename T>
class context {
    inline static thread_local std::vector<const T*> stack_;
public:
    struct provider {
        explicit provider(const T& v) { stack_.push_back(&v); }
        ~provider() { stack_.pop_back(); }
        provider(const provider&) = delete;
        provider& operator=(const provider&) = delete;
    };

    static const T& current() {
        if (stack_.empty())
            throw std::runtime_error("weasel::context: no provider in scope");
        return *stack_.back();
    }
};

} // namespace weasel
