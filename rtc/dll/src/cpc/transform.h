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

// --- 1) placeholder tag ---
namespace ph { 
    struct _1 {}; 
    struct _2 {};
}

// --- 2) a simple typelist ---
namespace tl {

    template<class... Ts>
    struct type_list {};

    template <typename TL1, typename TL2>
    struct jv2;

    template <typename ...Args1, typename ...Args2>
    struct jv2<type_list<Args1...>, type_list<Args2...> > {
        using type = type_list<Args1..., Args2...>;
    };

    template <typename TL1, typename TL2>
    using jv2_t = jv2<TL1, TL2>::type;

//    template <typename TL1, typename TL2, typename TL3, typename TL4> using jv4_t = jv2_t<jv2_t<TL1, TL2>, jv2_t<TL3, TL4> >;



    // --- 3) recursive substitution: replace every occurrence of Target with With ---
    // default: no change
    template<class Target, class With, class X>
    struct subst { using type = X; };

    // exact match -> With
    template<class Target, class With>
    struct subst<Target, With, Target> { using type = With; };

    // template recursion: C<Args...> -> C< subst<Args>... >
    template<class Target, class With, template<class...> class C, class... Args>
    struct subst<Target, With, C<Args...>> {
        using type = C<typename subst<Target, With, Args>::type...>;
    };

    template<class Target, class With, class X>
    using subst_t = typename subst<Target, With, X>::type;

    // --- 4) bind_placeholders: make a unary and binary metafunction from a template pattern ---
    template<template<class...> class C, class... PatternArgs>
    struct bind_placeholders {
        template<class T>
        using apply = C<subst_t<ph::_1, T, PatternArgs>...>;

        template<class T, class U>
        using apply2 = C<subst_t<ph::_1, T, subst_t<ph::_2, U, PatternArgs>>...>;
    };

    // Optional nicety: concept for "callable" unary meta
    template<class F, class T>
    concept UnaryMeta = requires { typename F::template apply<T>; };

    // --- 5) transform: map F over a type_list ---
    template<class List, class F>
    struct transform;

    template<class F, class... Ts>
        requires (UnaryMeta<F, Ts> && ...)
    struct transform<type_list<Ts...>, F> {
        using type = type_list<typename F::template apply<Ts>...>;
    };

    // Helper alias
    template<class List, class F>
    using transform_t = typename transform<List, F>::type;

    template<class List, template <typename Arg> class F>
    using transform_templ = transform_t<List, bind_placeholders<F, ph::_1> >;

} // namespace tl

#endif // __RTC_CPC_TRANSFORM_H
