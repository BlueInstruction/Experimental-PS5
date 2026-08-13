// SPDX-License-Identifier: MIT
// std::atomic_ref polyfill for Android NDK libc++.
//
// Problem: NDK 26 (LLVM 17) and NDK 27 (LLVM 18) ship a libc++ that does NOT
// export std::atomic_ref<T> in <atomic>, even when compiling with -std=c++20.
// FEXCore headers (SpinWaitLock.h, WritePriorityMutex.h, ...) use it heavily.
//
// Fix: This header force-defines a minimal std::atomic_ref<T> when the libc++
// feature-test macro __cpp_lib_atomic_ref is not defined. It uses GCC/Clang
// __atomic_* builtins, which work for any trivially-copyable type and resolve
// to the same lock-free instructions a real std::atomic_ref would use.
//
// Usage: force-include this header in every translation unit by passing
//   -include <path>/std_atomic_ref_polyfill.h
// to the compiler for all targets (px5 + FEXCore + FEXCore_Base + JemallocDummy).
//
// This file is intentionally minimal — it covers load/store/exchange/CAS and
// fetch_* arithmetic for integral types. wait/notify_one/notify_all are
// stubbed (no-op) since FEX's waiters fall back to futex syscalls when the
// libc++ implementations are unavailable.

#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>

#if !defined(__cpp_lib_atomic_ref) && !defined(_PX5_ATOMIC_REF_POLYFILL_DONE)
#define _PX5_ATOMIC_REF_POLYFILL_DONE
#define __cpp_lib_atomic_ref 201806L

namespace std {

template<typename T>
class atomic_ref {
public:
    using value_type = T;
    using difference_type = typename conditional<is_integral<T>::value, T, ptrdiff_t>::type;

    static constexpr bool is_always_lock_free =
        __atomic_always_lock_free(sizeof(T), nullptr);

    explicit atomic_ref(T& obj) noexcept : ptr_(&obj) {}
    atomic_ref(const atomic_ref&) noexcept = default;
    atomic_ref& operator=(const atomic_ref&) = delete;

    void store(T desired, memory_order order = memory_order_seq_cst) const noexcept {
        __atomic_store(ptr_, &desired, static_cast<int>(order));
    }

    T operator=(T desired) const noexcept {
        store(desired);
        return desired;
    }

    T load(memory_order order = memory_order_seq_cst) const noexcept {
        T val;
        __atomic_load(ptr_, &val, static_cast<int>(order));
        return val;
    }

    operator T() const noexcept {
        return load();
    }

    T exchange(T desired, memory_order order = memory_order_seq_cst) const noexcept {
        T old;
        __atomic_exchange(ptr_, &desired, &old, static_cast<int>(order));
        return old;
    }

    bool compare_exchange_weak(T& expected, T desired,
                                memory_order success,
                                memory_order failure) const noexcept {
        return __atomic_compare_exchange(ptr_, &expected, &desired, true,
                                         static_cast<int>(success),
                                         static_cast<int>(failure));
    }
    bool compare_exchange_weak(T& expected, T desired,
                                memory_order order = memory_order_seq_cst) const noexcept {
        return compare_exchange_weak(expected, desired, order, order);
    }

    bool compare_exchange_strong(T& expected, T desired,
                                  memory_order success,
                                  memory_order failure) const noexcept {
        return __atomic_compare_exchange(ptr_, &expected, &desired, false,
                                         static_cast<int>(success),
                                         static_cast<int>(failure));
    }
    bool compare_exchange_strong(T& expected, T desired,
                                  memory_order order = memory_order_seq_cst) const noexcept {
        return compare_exchange_strong(expected, desired, order, order);
    }

    // wait/notify: stubbed. FEX's SpinWaitLock already has a WFE-based fallback
    // for ARM and a spin-loop for other architectures; the libc++ atomic_ref
    // wait/notify is only used in code paths that already have a fallback.
    void wait(T, memory_order = memory_order_seq_cst) const noexcept {}
    void notify_one() const noexcept {}
    void notify_all() const noexcept {}

private:
    T* ptr_;
};

// fetch_* arithmetic for integral types. We enable these via a partial
// specialization on is_integral<T>. fetch_* on non-integral types is a
// compile error in std::atomic_ref, so the primary template correctly omits them.
template<typename T>
class atomic_ref_integral_base {
public:
    using value_type = T;
    using difference_type = T;

    T fetch_add(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_fetch_add(static_cast<T*>(ptr_), arg, static_cast<int>(order));
    }
    T fetch_sub(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_fetch_sub(static_cast<T*>(ptr_), arg, static_cast<int>(order));
    }
    T fetch_and(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_fetch_and(static_cast<T*>(ptr_), arg, static_cast<int>(order));
    }
    T fetch_or(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_fetch_or(static_cast<T*>(ptr_), arg, static_cast<int>(order));
    }
    T fetch_xor(T arg, memory_order order = memory_order_seq_cst) const noexcept {
        return __atomic_fetch_xor(static_cast<T*>(ptr_), arg, static_cast<int>(order));
    }

    T operator++(int) const noexcept { return fetch_add(T(1)); }
    T operator--(int) const noexcept { return fetch_sub(T(1)); }
    T operator++() const noexcept { return fetch_add(T(1)) + T(1); }
    T operator--() const noexcept { return fetch_sub(T(1)) - T(1); }
    T operator+=(T arg) const noexcept { return fetch_add(arg) + arg; }
    T operator-=(T arg) const noexcept { return fetch_sub(arg) - arg; }
    T operator&=(T arg) const noexcept { return fetch_and(arg) & arg; }
    T operator|=(T arg) const noexcept { return fetch_or(arg) | arg; }
    T operator^=(T arg) const noexcept { return fetch_xor(arg) ^ arg; }

protected:
    explicit atomic_ref_integral_base(T* p) : ptr_(p) {}
    T* ptr_;
};

// Helper: define fetch_* specializations for each integral type we need.
// We do this with macro expansion to keep it terse.
#define _PX5_ATOMIC_REF_INT_SPEC(INTEGRAL_TYPE)                              \
    template<> class atomic_ref<INTEGRAL_TYPE> : public atomic_ref_integral_base<INTEGRAL_TYPE> { \
    public:                                                                  \
        using value_type = INTEGRAL_TYPE;                                    \
        using difference_type = INTEGRAL_TYPE;                               \
        static constexpr bool is_always_lock_free =                          \
            __atomic_always_lock_free(sizeof(INTEGRAL_TYPE), nullptr);       \
        explicit atomic_ref(INTEGRAL_TYPE& obj) noexcept                     \
            : atomic_ref_integral_base<INTEGRAL_TYPE>(&obj) {}               \
        atomic_ref(const atomic_ref&) noexcept = default;                    \
        atomic_ref& operator=(const atomic_ref&) = delete;                   \
        void store(INTEGRAL_TYPE desired,                                    \
                   memory_order order = memory_order_seq_cst) const noexcept {\
            __atomic_store(ptr_, &desired, static_cast<int>(order));         \
        }                                                                    \
        INTEGRAL_TYPE operator=(INTEGRAL_TYPE desired) const noexcept {      \
            store(desired); return desired;                                  \
        }                                                                    \
        INTEGRAL_TYPE load(memory_order order = memory_order_seq_cst) const noexcept { \
            INTEGRAL_TYPE val;                                               \
            __atomic_load(ptr_, &val, static_cast<int>(order));              \
            return val;                                                      \
        }                                                                    \
        operator INTEGRAL_TYPE() const noexcept { return load(); }           \
        INTEGRAL_TYPE exchange(INTEGRAL_TYPE desired,                        \
                                memory_order order = memory_order_seq_cst) const noexcept { \
            INTEGRAL_TYPE old;                                               \
            __atomic_exchange(ptr_, &desired, &old, static_cast<int>(order));\
            return old;                                                      \
        }                                                                    \
        bool compare_exchange_weak(INTEGRAL_TYPE& expected, INTEGRAL_TYPE desired, \
                                    memory_order success, memory_order failure) const noexcept { \
            return __atomic_compare_exchange(ptr_, &expected, &desired, true, \
                                             static_cast<int>(success),      \
                                             static_cast<int>(failure));    \
        }                                                                    \
        bool compare_exchange_weak(INTEGRAL_TYPE& expected, INTEGRAL_TYPE desired, \
                                    memory_order order = memory_order_seq_cst) const noexcept { \
            return compare_exchange_weak(expected, desired, order, order);   \
        }                                                                    \
        bool compare_exchange_strong(INTEGRAL_TYPE& expected, INTEGRAL_TYPE desired, \
                                      memory_order success, memory_order failure) const noexcept { \
            return __atomic_compare_exchange(ptr_, &expected, &desired, false, \
                                             static_cast<int>(success),      \
                                             static_cast<int>(failure));    \
        }                                                                    \
        bool compare_exchange_strong(INTEGRAL_TYPE& expected, INTEGRAL_TYPE desired, \
                                      memory_order order = memory_order_seq_cst) const noexcept { \
            return compare_exchange_strong(expected, desired, order, order); \
        }                                                                    \
        void wait(INTEGRAL_TYPE, memory_order = memory_order_seq_cst) const noexcept {} \
        void notify_one() const noexcept {}                                  \
        void notify_all() const noexcept {}                                  \
    };

_PX5_ATOMIC_REF_INT_SPEC(char)
_PX5_ATOMIC_REF_INT_SPEC(signed char)
_PX5_ATOMIC_REF_INT_SPEC(unsigned char)
_PX5_ATOMIC_REF_INT_SPEC(short)
_PX5_ATOMIC_REF_INT_SPEC(unsigned short)
_PX5_ATOMIC_REF_INT_SPEC(int)
_PX5_ATOMIC_REF_INT_SPEC(unsigned int)
_PX5_ATOMIC_REF_INT_SPEC(long)
_PX5_ATOMIC_REF_INT_SPEC(unsigned long)
_PX5_ATOMIC_REF_INT_SPEC(long long)
_PX5_ATOMIC_REF_INT_SPEC(unsigned long long)

#undef _PX5_ATOMIC_REF_INT_SPEC

}  // namespace std

#endif  // __cpp_lib_atomic_ref
