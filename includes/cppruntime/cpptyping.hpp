/**
 * @file cpptyping.hpp
 * @brief C++ runtime typing
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TYPING_H
#define ___TYPING_H 0

#include <types.h>

#ifdef __cplusplus

namespace tosccp {

template <class T, T v>
struct integral_constant {
    static constexpr T value = v;
    typedef T                 value_type;
    typedef integral_constant type;
    constexpr operator value_type() const noexcept {
        return value;
    }
    constexpr value_type operator()() const noexcept {
        return value;
    }
};

typedef integral_constant<boolean_t, true>  true_type;
typedef integral_constant<boolean_t, false> false_type;

template <class A, class B>
struct is_same : false_type {};

template <class A>
struct is_same<A, A> : true_type {};

template <class T>
struct remove_const {
    typedef T type;
};

template <class T>
struct remove_const<const T> {
    typedef T type;
};

template <class T>
struct remove_volatile {
    typedef T type;
};

template <class T>
struct remove_volatile<volatile T> {
    typedef T type;
};

template <class T>
struct remove_cv {
    typedef typename remove_const<typename remove_volatile<T>::type>::type type;
};

template <class T>
struct remove_reference {
    typedef T type;
};

template <class T>
struct remove_reference<T &> {
    typedef T type;
};

template <class T>
struct remove_reference<T &&> {
    typedef T type;
};

template <class T>
struct remove_pointer {
    typedef T type;
};

template <class T>
struct remove_pointer<T*> {
    typedef T type;
};

template <class T>
struct remove_pointer<T*const> {
    typedef T type;
};

template <class T>
struct remove_pointer<T*volatile> {
    typedef T type;
};

template <class T>
struct remove_pointer<T*const volatile> {
    typedef T type;
};

template <class T>
struct remove_pointer<T*const volatile &> {
    typedef T type;
};

template <class T>
struct remove_pointer<T*const volatile &&> {
    typedef T type;
};

}

#else
#error "C++ compiler required"
#endif

#endif
