// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_CPC_TRANSFORM_H)
#define __RTC_CPC_TRANSFORM_H

#include <type_traits>
#include <tuple>
#include <utility>


// 1) Placeholder tags for simple template pattern binding.
//    - ph::_1 and ph::_2 act like positional placeholders within patterns.
//    - Used by bind_placeholders to substitute actual types into a template.

namespace ph {
    struct _1 {};
    struct _2 {};
}

namespace tl {


    // 2) A minimal typelist and concatenation helper.
    // 
    // A simple variadic type list container.

    template<class... Ts>
    struct type_list {};

    // jv2: join/concatenate two type_lists.
    // Primary template declaration.
    template <typename TL1, typename TL2>
    struct jv2;

    // Specialization for concatenating two type_list packs.
    template <typename ...Args1, typename ...Args2>
    struct jv2<type_list<Args1...>, type_list<Args2...>> {
        using type = type_list<Args1..., Args2...>;
    };

    // Helper alias: concatenated type_list for two lists.
    template <typename TL1, typename TL2>
    using jv2_t = typename jv2<TL1, TL2>::type;


    // 3) Recursive substitution: replace every occurrence of Target with With in X.
    // Default: no change.

    template<class Target, class With, class X>
    struct subst { using type = X; };

    // Exact match: replace Target with With.
    template<class Target, class With>
    struct subst<Target, With, Target> { using type = With; };

    // Template recursion: C<Args...> -> C< subst<Args>... >
    // Note: this only recurses through type template parameters. It won't see NTTPs.
    template<class Target, class With, template<class...> class C, class... Args>
    struct subst<Target, With, C<Args...>> {
        using type = C<typename subst<Target, With, Args>::type...>;
    };

    // Convenience alias.
    template<class Target, class With, class X>
    using subst_t = typename subst<Target, With, X>::type;


    // 4) bind_placeholders: turn a template pattern into a unary/binary metafunction class.
    // Example:
    //   using add_ptr_to = bind_placeholders<std::add_pointer_t, ph::_1>;
    //   using result = add_ptr_to::apply<int>; // int*

    template<template<class...> class C, class... PatternArgs>
    struct bind_placeholders {
        // Unary application: substitute _1 by T in each PatternArg, then apply C.
        template<class T>
        using apply = C<subst_t<ph::_1, T, PatternArgs>...>;

        // Binary application: substitute _1 by T and _2 by U in each PatternArg, then apply C.
        template<class T, class U>
        using apply2 = C<subst_t<ph::_1, T, subst_t<ph::_2, U, PatternArgs>>...>;
    };

    // concept: checks if F::apply<T> is a valid type.
    template<class F, class T>
    concept UnaryMeta = requires { typename F::template apply<T>; };

    template<class F, class T, class U>
    concept BinaryMeta = requires { typename F::template apply2<T, U>; };


    // 5) transform: map a metafunction over a type_list.
    // F must be a 'metafunction class' exposing 'template<class> using apply = ...;'

    template<class List, class F>
    struct transform;

    // Variadic expansion over type_list elements.
    template<class F, class... Ts>
        requires (UnaryMeta<F, Ts> && ...)
    struct transform<type_list<Ts...>, F> {
        using type = type_list<typename F::template apply<Ts>...>;
    };

    // Helper alias to get transformed list directly.
    template<class List, class F>
    using transform_t = typename transform<List, F>::type;

    // transform where F is a template< class Arg > class/alias; wrap with placeholder binder.
    template<class List, template <typename Arg> class F>
    using transform_templ = transform_t<List, bind_placeholders<F, ph::_1> >;


    // 6) Left-fold over a typelist.
    // fold<type_list<Ts...>, State, F> computes: F<Tn, F<Tn-1, ... F<T1, State>...>>
    //
    // IMPORTANT CONTRACT:
    // - F is expected to be a binary template alias/class that directly yields the next State type:
    //     template<class X, class Acc> using F = /* next-state type */;
    //   i.e. NOT a metafunction with nested ::type. If you have one, wrap it as:
    //     template<class X, class Acc> using F_wrap = typename F_meta<X, Acc>::type;

    template<class List, class State, template<class, class> class F>
    struct fold;

    // Base case: empty list yields the current State.
    template<class State, template<class, class> class F>
    struct fold<tl::type_list<>, State, F> { using type = State; };

    // Inductive case: peel Head, produce NextState = F<Head, State>, continue with Tail.
    template<class Head, class... Tail, class State, template<class, class> class F>
    struct fold<tl::type_list<Head, Tail...>, State, F> {
        using type =
            typename fold<tl::type_list<Tail...>, F<Head, State>, F>::type;
    };

    // Helper alias to get the folded result directly.
    template<class List, class State, template<class, class> class F>
    using fold_t = typename fold<List, State, F>::type;


    // 7) empty_base: utility base to enable EBO and seed a Visit symbol name.

    struct empty_base
    {
        // Declaration only; never defined nor called.
        template<class T> void Visit(const T*) const = delete;
    };

} // namespace tl

#endif // __RTC_CPC_TRANSFORM_H
