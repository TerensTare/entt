#ifndef ENTT_COMMON_THROWING_ALLOCATOR_HPP
#define ENTT_COMMON_THROWING_ALLOCATOR_HPP

#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>
#include <entt/container/dense_map.hpp>
#include <entt/core/allocator.hpp>
#include <entt/core/fwd.hpp>
#include <entt/core/type_info.hpp>

namespace test {

struct throwing_memory_stream_exception {};

template<typename Type>
class throwing_memory_stream final: public entt::memory_stream {
    using node_allocator = entt::stream_allocator<std::pair<const entt::id_type, std::size_t>>;

public:
    using container_type = entt::dense_map<entt::id_type, std::size_t>;

    throwing_memory_stream()
        : config{std::allocate_shared<container_type>(node_allocator{})} {}

    template<typename Other>
    throwing_memory_stream(const throwing_memory_stream<Type> &other)
        : config{other.config} {}

    void *allocate(std::size_t length, std::size_t align, entt::allocation_flags flags) override {
        if(const auto hash = entt::type_id<Type>().hash(); config->contains(hash)) {
            if(auto &elem = (*config)[hash]; elem == 0u) {
                config->erase(hash);
                throw throwing_memory_stream_exception{};
            } else {
                --elem;
            }
        }

        return allocator.allocate(length);
    }

    void deallocate(void *mem, std::size_t length, std::size_t align) override {
    }

    bool is_equal_with(const memory_stream &other) const noexcept override {
        return dynamic_cast<const throwing_memory_stream<Type> *>(&other) != nullptr;
    }

    template<typename Other>
    void throw_counter(const std::size_t len) {
        (*config)[entt::type_id<Other>().hash()] = len;
    }

    bool operator==(const throwing_allocator<Type> &) const {
        return true;
    }

    bool operator!=(const throwing_allocator<Type> &other) const {
        return !(*this == other);
    }

private:
    std::allocator<Type> allocator;
    std::shared_ptr<container_type> config;
};

} // namespace test

#endif
