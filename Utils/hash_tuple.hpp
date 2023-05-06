#include <tuple>
#include <type_traits>

// based on https://stackoverflow.com/questions/7110301/generic-hash-for-tuples-in-unordered-map-unordered-set

namespace hash_tuple
{
namespace
{
// does T have a member function asTuple()
template<typename T> constexpr std::remove_reference_t<decltype(T{}.asTuple(), bool{}) > has_asTuple() { return true; }
template<typename T, class... U> constexpr bool has_asTuple(U...) { return false; }
}

template <typename TT>
struct hash
{
    size_t operator()(TT const& tt) const
    {
        if constexpr (std::is_enum_v<TT>) {
            return std::hash<std::size_t>()(static_cast<std::size_t>(tt));
        }
        else if constexpr (has_asTuple<TT>()) {
            return hash<decltype(tt.asTuple())>()(tt.asTuple());
        }
        else {
            return std::hash<std::decay<TT>>()(tt);
        }
    }
};

namespace
{
template <class T>
inline void hash_combine(std::size_t& seed, T const& v)
{
    seed ^= hash_tuple::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Recursive template code derived from Matthieu M.
template <class Tuple, size_t Index = std::tuple_size<Tuple>::value>
struct HashValueImpl
{
    static void apply(size_t& seed, Tuple const& tuple)
    {
        HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
        hash_combine(seed, std::get<Index - 1>(tuple));
    }
};

template <class Tuple>
struct HashValueImpl<Tuple, 1>
{
    static void apply(size_t& seed, Tuple const& tuple)
    {
        hash_combine(seed, std::get<0>(tuple));
    }
};
template <class Tuple>
struct HashValueImpl<Tuple, 0>
{
    static void apply(size_t& seed, Tuple const& tuple)
    {
        //empty
    }
};
}

template <typename ... TT>
struct hash<std::tuple<TT...>>
{
    size_t operator()(std::tuple<TT...> const& tt) const
    {
        size_t seed = 0;
        HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
        return seed;
    }
};


// CRTP
// adds hash struct for hashing
template<typename Tupelable>
struct TupleHash {
    struct hash {
        size_t operator()(Tupelable const& tt) const {
            hash_tuple::hash<decltype(tt.asTuple())> h;
            return h(tt.asTuple());
        }
    };
};

}
