#ifndef ENTT_GRAPH_FLOW_HPP
#define ENTT_GRAPH_FLOW_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include "../config/config.h"
#include "../container/dense_map.hpp"
#include "../container/dense_set.hpp"
#include "../core/compressed_pair.hpp"
#include "../core/fwd.hpp"
#include "../core/iterator.hpp"
#include "../core/utility.hpp"
#include "adjacency_matrix.hpp"
#include "fwd.hpp"

namespace entt {

/**
 * @brief Utility class for creating task graphs.
 */
class basic_flow {
    using task_container_type = dense_set<id_type, identity, std::equal_to<id_type>, stream_allocator<id_type>>;
    using ro_rw_container_type = std::vector<std::pair<std::size_t, bool>, stream_allocator<std::pair<std::size_t, bool>>>;
    using deps_container_type = dense_map<id_type, ro_rw_container_type, identity, std::equal_to<id_type>, stream_allocator<std::pair<const id_type, ro_rw_container_type>>>;
    using adjacency_matrix_type = adjacency_matrix<directed_tag>;

    void emplace(const id_type res, const bool is_rw) {
        ENTT_ASSERT(index.first() < vertices.size(), "Invalid node");

        if(!deps.contains(res) && sync_on != vertices.size()) {
            deps[res].emplace_back(sync_on, true);
        }

        deps[res].emplace_back(index.first(), is_rw);
    }

    void setup_graph(adjacency_matrix_type &matrix) const {
        for(const auto &elem: deps) {
            const auto last = elem.second.cend();
            auto it = elem.second.cbegin();

            while(it != last) {
                if(it->second) {
                    // rw item
                    if(auto curr = it++; it != last) {
                        if(it->second) {
                            matrix.insert(curr->first, it->first);
                        } else if(const auto next = std::find_if(it, last, [](const auto &value) { return value.second; }); next != last) {
                            for(; it != next; ++it) {
                                matrix.insert(curr->first, it->first);
                                matrix.insert(it->first, next->first);
                            }
                        } else {
                            for(; it != next; ++it) {
                                matrix.insert(curr->first, it->first);
                            }
                        }
                    }
                } else {
                    // ro item (first iteration only)
                    if(const auto next = std::find_if(it, last, [](const auto &value) { return value.second; }); next != last) {
                        for(; it != next; ++it) {
                            matrix.insert(it->first, next->first);
                        }
                    } else {
                        it = last;
                    }
                }
            }
        }
    }

    void transitive_closure(adjacency_matrix_type &matrix) const {
        const auto length = matrix.size();

        for(std::size_t vk{}; vk < length; ++vk) {
            for(std::size_t vi{}; vi < length; ++vi) {
                for(std::size_t vj{}; vj < length; ++vj) {
                    if(matrix.contains(vi, vk) && matrix.contains(vk, vj)) {
                        matrix.insert(vi, vj);
                    }
                }
            }
        }
    }

    void transitive_reduction(adjacency_matrix_type &matrix) const {
        const auto length = matrix.size();

        for(std::size_t vert{}; vert < length; ++vert) {
            matrix.erase(vert, vert);
        }

        for(std::size_t vj{}; vj < length; ++vj) {
            for(std::size_t vi{}; vi < length; ++vi) {
                if(matrix.contains(vi, vj)) {
                    for(std::size_t vk{}; vk < length; ++vk) {
                        if(matrix.contains(vj, vk)) {
                            matrix.erase(vi, vk);
                        }
                    }
                }
            }
        }
    }

public:
    /*! @brief Allocator type. */
    using allocator_type = stream_allocator<id_type>;
    /*! @brief Unsigned integer type. */
    using size_type = std::size_t;
    /*! @brief Iterable task list. */
    using iterable = iterable_adaptor<typename task_container_type::const_iterator>;
    /*! @brief Adjacency matrix type. */
    using graph_type = adjacency_matrix_type;

    /*! @brief Default constructor. */
    basic_flow()
        : index{0u, allocator_type{}},
          vertices{},
          deps{},
          sync_on{} {}

    /*! @brief Default copy constructor. */
    basic_flow(const basic_flow &) = default;

    /*! @brief Default move constructor. */
    basic_flow(basic_flow &&) noexcept = default;

    /**
     * @brief Default copy assignment operator.
     * @return This flow builder.
     */
    basic_flow &operator=(const basic_flow &) = default;

    /**
     * @brief Default move assignment operator.
     * @return This flow builder.
     */
    basic_flow &operator=(basic_flow &&) noexcept = default;

    /**
     * @brief Returns the identifier at specified location.
     * @param pos Position of the identifier to return.
     * @return The requested identifier.
     */
    [[nodiscard]] id_type operator[](const size_type pos) const {
        return vertices.cbegin()[pos];
    }

    /*! @brief Clears the flow builder. */
    void clear() noexcept {
        index.first() = {};
        vertices.clear();
        deps.clear();
        sync_on = {};
    }

    /**
     * @brief Exchanges the contents with those of a given flow builder.
     * @param other Flow builder to exchange the content with.
     */
    void swap(basic_flow &other) {
        using std::swap;
        std::swap(index, other.index);
        std::swap(vertices, other.vertices);
        std::swap(deps, other.deps);
        std::swap(sync_on, other.sync_on);
    }

    /**
     * @brief Returns true if a flow builder contains no tasks, false otherwise.
     * @return True if the flow builder contains no tasks, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty();
    }

    /**
     * @brief Returns the number of tasks.
     * @return The number of tasks.
     */
    [[nodiscard]] size_type size() const noexcept {
        return vertices.size();
    }

    /**
     * @brief Binds a task to a flow builder.
     * @param value Task identifier.
     * @return This flow builder.
     */
    basic_flow &bind(const id_type value) {
        sync_on += (sync_on == vertices.size());
        const auto it = vertices.emplace(value).first;
        index.first() = size_type(it - vertices.begin());
        return *this;
    }

    /**
     * @brief Turns the current task into a sync point.
     * @return This flow builder.
     */
    basic_flow &sync() {
        ENTT_ASSERT(index.first() < vertices.size(), "Invalid node");
        sync_on = index.first();

        for(const auto &elem: deps) {
            elem.second.emplace_back(sync_on, true);
        }

        return *this;
    }

    /**
     * @brief Assigns a resource to the current task with a given access mode.
     * @param res Resource identifier.
     * @param is_rw Access mode.
     * @return This flow builder.
     */
    basic_flow &set(const id_type res, bool is_rw = false) {
        emplace(res, is_rw);
        return *this;
    }

    /**
     * @brief Assigns a read-only resource to the current task.
     * @param res Resource identifier.
     * @return This flow builder.
     */
    basic_flow &ro(const id_type res) {
        emplace(res, false);
        return *this;
    }

    /**
     * @brief Assigns a range of read-only resources to the current task.
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of elements.
     * @param last An iterator past the last element of the range of elements.
     * @return This flow builder.
     */
    template<typename It>
    std::enable_if_t<std::is_same_v<std::remove_const_t<typename std::iterator_traits<It>::value_type>, id_type>, basic_flow &>
    ro(It first, It last) {
        for(; first != last; ++first) {
            emplace(*first, false);
        }

        return *this;
    }

    /**
     * @brief Assigns a writable resource to the current task.
     * @param res Resource identifier.
     * @return This flow builder.
     */
    basic_flow &rw(const id_type res) {
        emplace(res, true);
        return *this;
    }

    /**
     * @brief Assigns a range of writable resources to the current task.
     * @tparam It Type of input iterator.
     * @param first An iterator to the first element of the range of elements.
     * @param last An iterator past the last element of the range of elements.
     * @return This flow builder.
     */
    template<typename It>
    std::enable_if_t<std::is_same_v<std::remove_const_t<typename std::iterator_traits<It>::value_type>, id_type>, basic_flow &>
    rw(It first, It last) {
        for(; first != last; ++first) {
            emplace(*first, true);
        }

        return *this;
    }

    /**
     * @brief Generates a task graph for the current content.
     * @return The adjacency matrix of the task graph.
     */
    [[nodiscard]] graph_type graph() const {
        graph_type matrix{vertices.size()};

        setup_graph(matrix);
        transitive_closure(matrix);
        transitive_reduction(matrix);

        return matrix;
    }

private:
    compressed_pair<size_type, allocator_type> index;
    task_container_type vertices;
    deps_container_type deps;
    size_type sync_on;
};

} // namespace entt

#endif
