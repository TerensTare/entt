#ifndef ENTT_ALLOCATOR_HPP
#define ENTT_ALLOCATOR_HPP

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace entt {
enum class allocation_flags {
    temporary_allocation = 1 << 0,
    permanent_allocation = 1 << 1,
};

struct memory_stream {
    virtual ~memory_stream() {}

    virtual void *allocate(std::size_t n, std::size_t align, allocation_flags flags = {}) = 0;
    virtual void deallocate(void *ptr, std::size_t n, std::size_t align) = 0;

    virtual bool is_equal_with(memory_stream const &other) const noexcept = 0;
};

struct new_delete_stream final: memory_stream {
    inline static memory_stream &instance() {
        static new_delete_stream stream;
        return stream;
    }

    void *allocate(std::size_t n, std::size_t align, allocation_flags flags) override {
        return ::operator new(n, static_cast<std::align_val_t>(align));
    }

    void deallocate(void *ptr, std::size_t n, std::size_t align) override {
        ::operator delete(ptr, n, static_cast<std::align_val_t>(align));
    }

    bool is_equal_with(memory_stream const &other) const noexcept override {
        return dynamic_cast<new_delete_stream const *>(&other) != nullptr;
    }

private:
    inline new_delete_stream() = default;
};

namespace internal {
inline static memory_stream *default_memory_stream = &new_delete_stream::instance();
} // namespace internal

memory_stream *get_memory_stream() {
    return internal::default_memory_stream;
}

memory_stream *set_memory_stream(memory_stream *stream) {
    return std::exchange(internal::default_memory_stream, stream);
}

template<typename T, typename... Args>
T *allocate_construct(memory_stream *stream, allocation_flags flags, Args &&...args) {
    static_assert(std::is_constructible_v<T, Args &&...>, "Cannot construct this type with the given params!");

    auto *ptr = stream->allocate(sizeof(T), alignof(T), flags);
    return new(ptr) T{std::forward<Args>(args)...};
}

template<typename T>
void deallocate_destroy(memory_stream *stream, T *ptr) {
    ptr->~T();
    stream->deallocate(ptr, sizeof(T), alignof(T));
}

template<typename T>
struct stream_allocator final {
    using value_type = T;

    constexpr stream_allocator() noexcept {}

    template<typename U>
    constexpr stream_allocator(const stream_allocator<U> &) noexcept {}

    T *allocate(std::size_t n) {
        return static_cast<T *>(get_memory_stream()->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T *ptr, std::size_t n) {
        get_memory_stream()->deallocate(ptr, n * sizeof(T), alignof(T));
    }

    template<typename U>
    constexpr bool operator==(const stream_allocator<U> &other) const noexcept {
        return get_memory_stream()->is_equal_with(*other.stream);
    }

    template<typename U>
    constexpr bool operator!=(const stream_allocator<U> &other) const noexcept {
        return !(*this == other);
    }
};

template<typename Stream>
struct scoped_use_memory_stream final {
    static_assert(std::is_base_of_v<memory_stream, Stream>, "Invalid memory stream");

    explicit scoped_use_memory_stream(Stream &stream)
        : old{set_memory_stream(&stream)} {}

    ~scoped_use_memory_stream() {
        set_memory_stream(old);
    }

private:
    memor_stream *old;
};
} // namespace entt

#endif // ENTT_ALLOCATOR_HPP