#ifndef ENTT_ENTITY_CONTEXT_HPP
#define ENTT_ENTITY_CONTEXT_HPP

#include "../container/dense_map.hpp"
#include "../core/any.hpp"

namespace entt {

/*! @cond TURN_OFF_DOXYGEN */
namespace internal {

template<typename Allocator>
class registry_context {
    using alloc_traits = std::allocator_traits<Allocator>;
    using allocator_type = typename alloc_traits::template rebind_alloc<std::pair<const id_type, basic_any<0u>>>;

public:
    explicit registry_context(const allocator_type &allocator)
        : ctx{allocator} {}

    template<typename Type, typename... Args>
    Type &emplace_as(const id_type id, Args &&...args) {
        return any_cast<Type &>(ctx.try_emplace(id, std::in_place_type<Type>, std::forward<Args>(args)...).first->second);
    }

    template<typename Type, typename... Args>
    Type &emplace(Args &&...args) {
        return emplace_as<Type>(type_id<Type>().hash(), std::forward<Args>(args)...);
    }

    template<typename Type>
    Type &insert_or_assign(const id_type id, Type &&value) {
        return any_cast<std::remove_cv_t<std::remove_reference_t<Type>> &>(ctx.insert_or_assign(id, std::forward<Type>(value)).first->second);
    }

    template<typename Type>
    Type &insert_or_assign(Type &&value) {
        return insert_or_assign(type_id<Type>().hash(), std::forward<Type>(value));
    }

    template<typename Type>
    bool erase(const id_type id = type_id<Type>().hash()) {
        const auto it = ctx.find(id);
        return it != ctx.end() && it->second.type() == type_id<Type>() ? (ctx.erase(it), true) : false;
    }

    template<typename Type>
    [[nodiscard]] const Type &get(const id_type id = type_id<Type>().hash()) const {
        return any_cast<const Type &>(ctx.at(id));
    }

    template<typename Type>
    [[nodiscard]] Type &get(const id_type id = type_id<Type>().hash()) {
        return any_cast<Type &>(ctx.at(id));
    }

    template<typename Type>
    [[nodiscard]] const Type *find(const id_type id = type_id<Type>().hash()) const {
        const auto it = ctx.find(id);
        return it != ctx.cend() ? any_cast<const Type>(&it->second) : nullptr;
    }

    template<typename Type>
    [[nodiscard]] Type *find(const id_type id = type_id<Type>().hash()) {
        const auto it = ctx.find(id);
        return it != ctx.end() ? any_cast<Type>(&it->second) : nullptr;
    }

    template<typename Type>
    [[nodiscard]] bool contains(const id_type id = type_id<Type>().hash()) const {
        const auto it = ctx.find(id);
        return it != ctx.cend() && it->second.type() == type_id<Type>();
    }

private:
    dense_map<id_type, basic_any<0u>, identity, std::equal_to<>, allocator_type> ctx;
};

template<typename Lock, typename = void>
struct is_lockable: std::false_type {};

template<typename Lock>
struct is_lockable<Lock,
                   std::void_t<decltype(std::declval<Lock &>().lock(),
                                        std::declval<Lock &>().unlock())>>: std::true_type {};

template<typename Mutex>
struct unique_lock final {
    using mutex_type = Mutex;

    static_assert(is_lockable<mutex_type>::value, "Invalid mutex type!");

    explicit unique_lock(Mutex &mtx)
        : mtx{std::addressof(mtx)} {
        mtx.lock();
    }

    unique_lock(unique_lock const &) = delete;
    unique_lock &operator=(unique_lock const &) = delete;

    unique_lock(unique_lock &&other)
        : mtx{std::exchange(other.mtx, nullptr)} {}

    unique_lock &operator=(unique_lock &&other) {
        if(this != std::addressof(other)) {
            if(mtx) {
                mtx->unlock();
            }

            mtx = std::exchange(other.mtx, nullptr);
        }
        return *this;
    }

    ~unique_lock() {
        if(mtx) {
            mtx->unlock();
        }
    }

private:
    Mutex *mtx;
};

} // namespace internal
/*! @endcond */

/**
 * @brief Wrapper type used to add support for locking to the registry context.
 *
 * Both the context and mutex are passed and stored by reference, so make sure they are valid during the lifetime of this data.
 *
 * The mutex type must implement both lock() and unlock() to be valid.
 *
 * @tparam Context Underlying context type.
 * @tparam Mutex Underlying mutex type.
 */
template<typename Context, typename Mutex>
class locked_context final {
    using context_type = Context;
    using mutex_type = Mutex;
    using lock_type = internal::unique_lock<mutex_type>;

public:
    /**
     * @brief Constructs a locked context for a given context and lock.
     * @param ctx The reference to the context to use.
     * @param mtx The reference to the mutex to use.
     */
    locked_context(Context &ctx, Mutex &mtx)
        : ctx{&ctx},
          mtx{&mtx} {}

    /*! @brief A locked context cannot be copied. */
    locked_context(locked_context const &) = delete;

    /*! @brief A locked context cannot be copied. */
    locked_context &operator=(locked_context const &) = delete;

    /**
     * @brief Emplaces a new entry of given type to the given id using the given arguments.
     * @param id The id to associate the entry with.
     * @param args Parameters to construct the entry with.
     * @return A constructed instance of Type.
     */
    template<typename Type, typename... Args>
    Type &emplace_as(const id_type id, Args &&...args) {
        lock_type _(*mtx);
        return ctx->emplace_as<Type>(id, std::forward<Args>(args)...);
    }

    /**
     * @brief Emplaces a new entry of given type using the given arguments.
     * @param args Parameters to construct the entry with.
     * @return A constructed instance of Type.
     */
    template<typename Type, typename... Args>
    Type &emplace(Args &&...args) {
        lock_type _(*mtx);
        return ctx->emplace<Type>(std::forward<Args>(args)...);
    }

    /**
     * @brief Insert a new entry of given type or replace the existing one.
     * @param id The id associated with the entry to construct or replace.
     * @param value The value to assign to the entry in case it doesn't exist.
     * @return A reference to the data entry.
     */
    template<typename Type>
    Type &insert_or_assign(const id_type id, Type &&value) {
        lock_type _(*mtx);
        return ctx->insert_or_assign(id, std::forward<Type>(value));
    }

    /**
     * @brief Insert a new entry of given type or replace the existing one.
     * @param value The value to assign to the entry in case it doesn't exist.
     * @return A reference to the data entry.
     */
    template<typename Type>
    Type &insert_or_assign(Type &&value) {
        lock_type _(*mtx);
        return ctx->insert_or_assign(std::forward<Type>(value));
    }

    /**
     * @brief Erase the entry of given type (and optionally id) from the context.
     * @param id Optional name to use to map the entry within the context.
     * @return A bool denoting whether the erasure took place.
     */
    template<typename Type>
    bool erase(const id_type id = type_id<Type>().hash()) {
        lock_type _(*mtx);
        return ctx->erase<Type>(id);
    }

    /**
     * @brief Get an entry of the given type.
     * @param id Optional name to use to map the entry within the context.
     */
    template<typename Type>
    [[nodiscard]] const Type &get(const id_type id = type_id<Type>().hash()) const {
        lock_type _(*mtx);
        return ctx->get<Type>(id);
    }

    /*! @copydoc get */
    template<typename Type>
    [[nodiscard]] Type &get(const id_type id = type_id<Type>().hash()) {
        lock_type _(*mtx);
        return ctx->get<Type>(id);
    }

    /**
     * @brief Get a pointer to the data associated with an entry of the given type, if any.
     * @param id Optional name to use to map the entry within the context.
     */
    template<typename Type>
    [[nodiscard]] const Type *find(const id_type id = type_id<Type>().hash()) const {
        lock_type _(*mtx);
        return ctx->find<Type>(id);
    }

    /*! @copydoc find */
    template<typename Type>
    [[nodiscard]] Type *find(const id_type id = type_id<Type>().hash()) {
        lock_type _(*mtx);
        return ctx->find<Type>(id);
    }

    /**
     * @brief Check whether there exists an entry of the given type.
     * @param id Optional name to use to map the entry within the context.
     */
    template<typename Type>
    [[nodiscard]] bool contains(const id_type id = type_id<Type>().hash()) const {
        lock_type _(*mtx);
        return ctx->contains<Type>(id);
    }

private:
    Context *ctx;
    Mutex *mtx;
};

template<typename Context, typename Mutex>
locked_context(Context &, Mutex &) -> locked_context<Context, Mutex>;

} // namespace entt

#endif