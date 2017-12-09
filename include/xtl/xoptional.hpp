/***************************************************************************
* Copyright (c) 2016, Johan Mabille, Sylvain Corlay and Wolf Vollprecht    *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XTL_OPTIONAL_HPP
#define XTL_OPTIONAL_HPP

#include <cmath>
#include <ostream>
#include <type_traits>
#include <utility>

#include "xclosure.hpp"
#include "xmeta_utils.hpp"
#include "xtl_config.hpp"
#include "xtype_traits.hpp"

namespace xtl
{
    /********************
     * optional helpers *
     ********************/

    template <class T, class B>
    auto optional(T&& t, B&& b) noexcept;

    template <class T>
    auto missing() noexcept;

    /*************************
     * xoptional declaration *
     *************************/

    template <class CT, class CB = bool>
    class xoptional;

    namespace detail
    {
        template <class E>
        struct is_xoptional_impl : std::false_type
        {
        };

        template <class CT, class CB>
        struct is_xoptional_impl<xoptional<CT, CB>> : std::true_type
        {
        };

        template <class CT, class CTO, class CBO>
        using converts_from_xoptional = disjunction<
            std::is_constructible<CT, const xoptional<CTO, CBO>&>,
            std::is_constructible<CT, xoptional<CTO, CBO>&>,
            std::is_constructible<CT, const xoptional<CTO, CBO>&&>,
            std::is_constructible<CT, xoptional<CTO, CBO>&&>,
            std::is_convertible<const xoptional<CTO, CBO>&, CT>,
            std::is_convertible<xoptional<CTO, CBO>&, CT>,
            std::is_convertible<const xoptional<CTO, CBO>&&, CT>,
            std::is_convertible<xoptional<CTO, CBO>&&, CT>
        >;

        template <class CT, class CTO, class CBO>
        using assigns_from_xoptional = disjunction<
            std::is_assignable<std::add_lvalue_reference_t<CT>, const xoptional<CTO, CBO>&>,
            std::is_assignable<std::add_lvalue_reference_t<CT>, xoptional<CTO, CBO>&>,
            std::is_assignable<std::add_lvalue_reference_t<CT>, const xoptional<CTO, CBO>&&>,
            std::is_assignable<std::add_lvalue_reference_t<CT>, xoptional<CTO, CBO>&&>
        >;

        template <class... Args>
        struct common_optional_impl;

        template <class T>
        struct common_optional_impl<T>
        {
            using type = std::conditional_t < is_xoptional_impl<T>::value , T, xoptional<T >> ;
        };

        template <class T>
        struct identity
        {
            using type = T;
        };

        template <class T>
        struct get_value_type
        {
            using type = typename T::value_type;
        };

        template<class T1, class T2>
        struct common_optional_impl<T1, T2>
        {
            using decay_t1 = std::decay_t<T1>;
            using decay_t2 = std::decay_t<T2>;
            using type1 = xtl::mpl::eval_if_t<std::is_fundamental<decay_t1>, identity<decay_t1>, get_value_type<decay_t1>>;
            using type2 = xtl::mpl::eval_if_t<std::is_fundamental<decay_t2>, identity<decay_t2>, get_value_type<decay_t2>>;
            using type = xoptional<std::common_type_t<type1, type2>>;
        };

        template <class T1, class T2, class B2>
        struct common_optional_impl<T1, xoptional<T2, B2>>
            : common_optional_impl<T1, T2>
        {
        };

        template <class T1, class B1, class T2>
        struct common_optional_impl<xoptional<T1, B1>, T2>
            : common_optional_impl<T2, xoptional<T1, B1>>
        {
        };

        template <class T1, class B1, class T2, class B2>
        struct common_optional_impl<xoptional<T1, B1>, xoptional<T2, B2>>
            : common_optional_impl<T1, T2>
        {
        };

        template <class T1, class T2, class... Args>
        struct common_optional_impl<T1, T2, Args...>
        {
            using type = typename common_optional_impl<
                             typename common_optional_impl<T1, T2>::type,
                             Args...
                         >::type;
        };
    }

    template <class E>
    using is_xoptional = detail::is_xoptional_impl<E>;

    template <class E, class R = void>
    using disable_xoptional = std::enable_if_t<!is_xoptional<E>::value, R>;

    template <class E1, class E2, class R = void>
    using disable_both_xoptional = std::enable_if<!is_xoptional<E1>::value && !is_xoptional<E2>::value, R>;

    template <class E, class R = void>
    using enable_xoptional = std::enable_if_t<is_xoptional<E>::value, R>;

    template <class E1, class E2, class R = void>
    using enable_both_xoptional = std::enable_if<is_xoptional<E1>::value && is_xoptional<E2>::value, R>;

    template <class... Args>
    struct common_optional : detail::common_optional_impl<Args...>
    {
    };

    template <class... Args>
    using common_optional_t = typename common_optional<Args...>::type;

    /**
     * @class xoptional
     * @brief Optional value handler.
     *
     * The xoptional is an optional proxy. It holds a value (or a reference on a value) and a flag (or reference on a flag)
     * indicating whether the element should be considered missing.
     *
     * xoptional is different from std::optional
     *
     *  - no `operator->()` that returns a pointer.
     *  - no `operator*()` that returns a value.
     *
     * The only way to access the underlying value and flag is with the `value` and `value_or` methods.
     *
     *  - no explicit convertion to bool. This may lead to confusion when the underlying value type is boolean too.
     *
     * @tparam CT Closure type for the value.
     * @tparam CB Closure type for the missing flag. A falsy flag means that the value is missing.
     *
     * \ref xoptional is used both as a value type (with CT and CB being value types) and reference type for containers
     * with CT and CB being reference types. In other words, it serves as a reference proxy.
     *
     */
    template <class CT, class CB>
    class xoptional
    {
    public:

        using self_type = xoptional<CT, CB>;
        using value_closure = CT;
        using flag_closure = CB;

        using value_type = std::decay_t<CT>;
        using flag_type = std::decay_t<CB>;

        // Constructors
        inline xoptional()
            : m_value(), m_flag(false)
        {
        }

        template <class T,
          std::enable_if_t<
            conjunction<
              negation<std::is_same<xoptional<CT, CB>, std::decay_t<T>>>,
              std::is_constructible<CT, T&&>,
              std::is_convertible<T&&, CT>
            >::value,
            bool
          > = true>
        inline constexpr xoptional(T&& rhs)
            : m_value(std::forward<T>(rhs)), m_flag(true)
        {
        }

        template <class T,
          std::enable_if_t<
            conjunction<
              negation<std::is_same<xoptional<CT, CB>, std::decay_t<T>>>,
              std::is_constructible<CT, T&&>,
              negation<std::is_convertible<T&&, CT>>
            >::value,
            bool
          > = false>
        inline explicit constexpr xoptional(T&& value)
            : m_value(std::forward<T>(value)), m_flag(true)
        {
        }

        template <class CTO, class CBO,
          std::enable_if_t<
            conjunction<
              negation<std::is_same<xoptional<CT, CB>, xoptional<CTO, CBO>>>,
              std::is_constructible<CT, std::add_lvalue_reference_t<std::add_const_t<CTO>>>,
              std::is_constructible<CB, std::add_lvalue_reference_t<std::add_const_t<CBO>>>,
              conjunction<
                std::is_convertible<std::add_lvalue_reference_t<std::add_const_t<CTO>>, CT>,
                std::is_convertible<std::add_lvalue_reference_t<std::add_const_t<CBO>>, CB>
              >,
              negation<detail::converts_from_xoptional<CT, CTO, CBO>>
            >::value,
            bool
          > = true>
        inline constexpr xoptional(const xoptional<CTO, CBO>& rhs)
            : m_value(rhs.value()), m_flag(rhs.has_value())
        {
        }

        template <class CTO, class CBO,
          std::enable_if_t<
            conjunction<
              negation<std::is_same<xoptional<CT, CB>, xoptional<CTO, CBO>>>,
              std::is_constructible<CT, std::add_lvalue_reference_t<std::add_const_t<CTO>>>,
              std::is_constructible<CB, std::add_lvalue_reference_t<std::add_const_t<CBO>>>,
              disjunction<
                negation<std::is_convertible<std::add_lvalue_reference_t<std::add_const_t<CTO>>, CT>>,
                negation<std::is_convertible<std::add_lvalue_reference_t<std::add_const_t<CBO>>, CB>>
              >,
              negation<detail::converts_from_xoptional<CT, CTO, CBO>>
            >::value,
            bool
          > = false>
        inline explicit constexpr xoptional(const xoptional<CTO, CBO>& rhs)
            : m_value(rhs.value()), m_flag(rhs.has_value())
        {
        }

        template <class CTO, class CBO,
          std::enable_if_t<
            conjunction<
              negation<std::is_same<xoptional<CT, CB>, xoptional<CTO, CBO>>>,
              std::is_constructible<CT, std::conditional_t<std::is_reference<CT>::value, const std::decay_t<CTO>&, std::decay_t<CTO>&&>>,
              std::is_constructible<CB, std::conditional_t<std::is_reference<CB>::value, const std::decay_t<CBO>&, std::decay_t<CBO>&&>>,
              conjunction<
                std::is_convertible<std::conditional_t<std::is_reference<CT>::value, const std::decay_t<CTO>&, std::decay_t<CTO>&&>, CT>,
                std::is_convertible<std::conditional_t<std::is_reference<CB>::value, const std::decay_t<CBO>&, std::decay_t<CBO>&&>, CB>
              >,
              negation<detail::converts_from_xoptional<CT, CTO, CBO>>
            >::value,
            bool
          > = true>
        inline constexpr xoptional(xoptional<CTO, CBO>&& rhs)
            : m_value(std::move(rhs).value()), m_flag(std::move(rhs).has_value())
        {
        }

        template <class CTO, class CBO,
          std::enable_if_t<
            conjunction<
              negation<std::is_same<xoptional<CT, CB>, xoptional<CTO, CBO>>>,
              std::is_constructible<CT, std::conditional_t<std::is_reference<CT>::value, const std::decay_t<CTO>&, std::decay_t<CTO>&&>>,
              std::is_constructible<CB, std::conditional_t<std::is_reference<CB>::value, const std::decay_t<CBO>&, std::decay_t<CBO>&&>>,
              disjunction<
                negation<std::is_convertible<std::conditional_t<std::is_reference<CT>::value, const std::decay_t<CTO>&, std::decay_t<CTO>&&>, CT>>,
                negation<std::is_convertible<std::conditional_t<std::is_reference<CB>::value, const std::decay_t<CBO>&, std::decay_t<CBO>&&>, CB>>
              >,
              negation<detail::converts_from_xoptional<CT, CTO, CBO>>
            >::value,
            bool
          > = false>
        inline explicit constexpr xoptional(xoptional<CTO, CBO>&& rhs)
            : m_value(std::move(rhs).value()), m_flag(std::move(rhs).has_value())
        {
        }

        xoptional(value_type&&, flag_type&&);
        xoptional(std::add_lvalue_reference_t<CT>, std::add_lvalue_reference_t<CB>);
        xoptional(value_type&&, std::add_lvalue_reference_t<CB>);
        xoptional(std::add_lvalue_reference_t<CT>, flag_type&&);

        // Assignment
        template <class T>
        std::enable_if_t<
          conjunction<
            negation<std::is_same<xoptional<CT, CB>, std::decay_t<T>>>,
            std::is_assignable<std::add_lvalue_reference_t<CT>, T>
          >::value,
         xoptional&>
        inline operator=(T&& rhs)
        {
            m_flag = true;
            m_value = std::forward<T>(rhs);
            return *this;
        }

        template <class CTO, class CBO>
        std::enable_if_t<conjunction<
          negation<std::is_same<xoptional<CT, CB>, xoptional<CTO, CBO>>>,
          std::is_assignable<std::add_lvalue_reference_t<CT>, CTO>,
          negation<detail::converts_from_xoptional<CT, CTO, CBO>>,
          negation<detail::assigns_from_xoptional<CT, CTO, CBO>>
        >::value,
        xoptional&>
        inline operator=(const xoptional<CTO, CBO>& rhs)
        {
            m_flag = rhs.has_value();
            m_value = rhs.value();
            return *this;
        }

        template <class CTO, class CBO>
        std::enable_if_t<conjunction<
          negation<std::is_same<xoptional<CT, CB>, xoptional<CTO, CBO>>>,
          std::is_assignable<std::add_lvalue_reference_t<CT>, CTO>,
          negation<detail::converts_from_xoptional<CT, CTO, CBO>>,
          negation<detail::assigns_from_xoptional<CT, CTO, CBO>>
        >::value,
        xoptional&>
        inline operator=(xoptional<CTO, CBO>&& rhs)
        {
            m_flag = std::move(rhs).has_value();
            m_value = std::move(rhs).value();
            return *this;
        }

        // Operators
        template <class CTO, class CBO>
        xoptional& operator+=(const xoptional<CTO, CBO>&);
        template <class CTO, class CBO>
        xoptional& operator-=(const xoptional<CTO, CBO>&);
        template <class CTO, class CBO>
        xoptional& operator*=(const xoptional<CTO, CBO>&);
        template <class CTO, class CBO>
        xoptional& operator/=(const xoptional<CTO, CBO>&);

        template <class T>
        disable_xoptional<T, xoptional&> operator+=(const T&);
        template <class T>
        disable_xoptional<T, xoptional&> operator-=(const T&);
        template <class T>
        disable_xoptional<T, xoptional&> operator*=(const T&);
        template <class T>
        disable_xoptional<T, xoptional&> operator/=(const T&);

        // Access
        std::add_lvalue_reference_t<CT> value() & noexcept;
        std::add_lvalue_reference_t<std::add_const_t<CT>> value() const & noexcept;
        std::conditional_t<std::is_reference<CT>::value, apply_cv_t<CT, value_type>&, value_type> value() && noexcept;
        std::conditional_t<std::is_reference<CT>::value, const value_type&, value_type> value() const && noexcept;

        template <class U>
        value_type value_or(U&&) const & noexcept;
        template <class U>
        value_type value_or(U&&) const && noexcept;

        // Access
        std::add_lvalue_reference_t<CB> has_value() & noexcept;
        std::add_lvalue_reference_t<std::add_const_t<CB>> has_value() const & noexcept;
        std::conditional_t<std::is_reference<CB>::value, apply_cv_t<CB, flag_type>&, flag_type> has_value() && noexcept;
        std::conditional_t<std::is_reference<CB>::value, const flag_type&, flag_type> has_value() const && noexcept;

        // Swap
        void swap(xoptional& other);

        // Comparison
        template <class CTO, class CBO>
        bool equal(const xoptional<CTO, CBO>& rhs) const noexcept;

        template <class CTO>
        disable_xoptional<CTO, bool> equal(const CTO& rhs) const noexcept;

        xclosure_pointer<self_type&> operator&() &;
        xclosure_pointer<const self_type&> operator&() const &;
        xclosure_pointer<self_type> operator&() &&;

    private:

        template <class CTO, class CBO>
        friend class xoptional;

        CT m_value;
        CB m_flag;
    };

    // value

    template <class T, class U = disable_xoptional<std::decay_t<T>>>
    T&& value(T&& v)
    {
        return std::forward<T>(v);
    }

    template <class CT, class CB>
    decltype(auto) value(xtl::xoptional<CT, CB>&& v)
    {
        return std::move(v).value();
    }

    template <class CT, class CB>
    decltype(auto) value(xtl::xoptional<CT, CB>& v)
    {
        return v.value();
    }

    template <class CT, class CB>
    decltype(auto) value(const xtl::xoptional<CT, CB>& v)
    {
        return v.value();
    }

    // has_value

    template <class T, class U = disable_xoptional<std::decay_t<T>>>
    bool has_value(T&&)
    {
        return true;
    }

    template <class CT, class CB>
    decltype(auto) has_value(xtl::xoptional<CT, CB>&& v)
    {
        return std::move(v).has_value();
    }

    template <class CT, class CB>
    decltype(auto) has_value(xtl::xoptional<CT, CB>& v)
    {
        return std::move(v).has_value();
    }

    template <class CT, class CB>
    decltype(auto) has_value(const xtl::xoptional<CT, CB>& v)
    {
        return std::move(v).has_value();
    }

    /***************************************
     * optional and missing implementation *
     ***************************************/

    /**
     * @brief Returns an \ref xoptional holding closure types on the specified parameters
     *
     * @tparam t the optional value
     * @tparam b the boolean flag
     */
    template <class T, class B>
    inline auto optional(T&& t, B&& b) noexcept
    {
        using optional_type = xoptional<closure_type_t<T>, closure_type_t<B>>;
        return optional_type(std::forward<T>(t), std::forward<B>(b));
    }

    /**
     * @brief Returns an \ref xoptional for a missig value
     */
    template <class T>
    auto missing() noexcept
    {
        return xoptional<T, bool>(T(), false);
    }

    /****************************
     * xoptional implementation *
     ****************************/

    // Constructors
    template <class CT, class CB>
    xoptional<CT, CB>::xoptional(value_type&& value, flag_type&& flag)
        : m_value(std::move(value)), m_flag(std::move(flag))
    {
    }

    template <class CT, class CB>
    xoptional<CT, CB>::xoptional(std::add_lvalue_reference_t<CT> value, std::add_lvalue_reference_t<CB> flag)
        : m_value(value), m_flag(flag)
    {
    }

    template <class CT, class CB>
    xoptional<CT, CB>::xoptional(value_type&& value, std::add_lvalue_reference_t<CB> flag)
        : m_value(std::move(value)), m_flag(flag)
    {
    }

    template <class CT, class CB>
    xoptional<CT, CB>::xoptional(std::add_lvalue_reference_t<CT> value, flag_type&& flag)
        : m_value(value), m_flag(std::move(flag))
    {
    }

    // Operators
    template <class CT, class CB>
    template <class CTO, class CBO>
    auto xoptional<CT, CB>::operator+=(const xoptional<CTO, CBO>& rhs) -> xoptional&
    {
        m_flag = m_flag && rhs.m_flag;
        if (m_flag)
        {
            m_value += rhs.m_value;
        }
        return *this;
    }

    template <class CT, class CB>
    template <class CTO, class CBO>
    auto xoptional<CT, CB>::operator-=(const xoptional<CTO, CBO>& rhs) -> xoptional&
    {
        m_flag = m_flag && rhs.m_flag;
        if (m_flag)
        {
            m_value -= rhs.m_value;
        }
        return *this;
    }

    template <class CT, class CB>
    template <class CTO, class CBO>
    auto xoptional<CT, CB>::operator*=(const xoptional<CTO, CBO>& rhs) -> xoptional&
    {
        m_flag = m_flag && rhs.m_flag;
        if (m_flag)
        {
            m_value *= rhs.m_value;
        }
        return *this;
    }

    template <class CT, class CB>
    template <class CTO, class CBO>
    auto xoptional<CT, CB>::operator/=(const xoptional<CTO, CBO>& rhs) -> xoptional&
    {
        m_flag = m_flag && rhs.m_flag;
        if (m_flag)
        {
            m_value /= rhs.m_value;
        }
        return *this;
    }

    template <class CT, class CB>
    template <class T>
    auto xoptional<CT, CB>::operator+=(const T& rhs) -> disable_xoptional<T, xoptional&>
    {
        if (m_flag)
        {
            m_value += rhs;
        }
        return *this;
    }

    template <class CT, class CB>
    template <class T>
    auto xoptional<CT, CB>::operator-=(const T& rhs) -> disable_xoptional<T, xoptional&>
    {
        if (m_flag)
        {
            m_value -= rhs;
        }
        return *this;
    }

    template <class CT, class CB>
    template <class T>
    auto xoptional<CT, CB>::operator*=(const T& rhs) -> disable_xoptional<T, xoptional&>
    {
        if (m_flag)
        {
            m_value *= rhs;
        }
        return *this;
    }

    template <class CT, class CB>
    template <class T>
    auto xoptional<CT, CB>::operator/=(const T& rhs) -> disable_xoptional<T, xoptional&>
    {
        if (m_flag)
        {
            m_value /= rhs;
        }
        return *this;
    }

    // Access
    template <class CT, class CB>
    auto xoptional<CT, CB>::value() & noexcept -> std::add_lvalue_reference_t<CT>
    {
        return m_value;
    }

    template <class CT, class CB>
    auto xoptional<CT, CB>::value() const & noexcept -> std::add_lvalue_reference_t<std::add_const_t<CT>>
    {
        return m_value;
    }

    template <class CT, class CB>
    auto xoptional<CT, CB>::value() && noexcept -> std::conditional_t<std::is_reference<CT>::value, apply_cv_t<CT, value_type>&, value_type>
    {
        return m_value;
    }

    template <class CT, class CB>
    auto xoptional<CT, CB>::value() const && noexcept -> std::conditional_t<std::is_reference<CT>::value, const value_type&, value_type>
    {
        return m_value;
    }

    template <class CT, class CB>
    template <class U>
    auto xoptional<CT, CB>::value_or(U&& default_value) const & noexcept -> value_type
    {
        return m_flag ? m_value : std::forward<U>(default_value);
    }

    template <class CT, class CB>
    template <class U>
    auto xoptional<CT, CB>::value_or(U&& default_value) const && noexcept -> value_type
    {
        return m_flag ? m_value : std::forward<U>(default_value);
    }

    // Access
    template <class CT, class CB>
    auto xoptional<CT, CB>::has_value() & noexcept -> std::add_lvalue_reference_t<CB>
    {
        return m_flag;
    }

    template <class CT, class CB>
    auto xoptional<CT, CB>::has_value() const & noexcept -> std::add_lvalue_reference_t<std::add_const_t<CB>>
    {
        return m_flag;
    }

    template <class CT, class CB>
    auto xoptional<CT, CB>::has_value() && noexcept -> std::conditional_t<std::is_reference<CB>::value, apply_cv_t<CB, flag_type>&, flag_type>
    {
        return m_flag;
    }

    template <class CT, class CB>
    auto xoptional<CT, CB>::has_value() const && noexcept -> std::conditional_t<std::is_reference<CB>::value, const flag_type&, flag_type>
    {
        return m_flag;
    }

    // Swap
    template <class CT, class CB>
    void xoptional<CT, CB>::swap(xoptional& other)
    {
        std::swap(m_value, other.m_flag);
        std::swap(m_flag, other.m_flag);
    }

    // Comparison
    template <class CT, class CB>
    template <class CTO, class CBO>
    auto xoptional<CT, CB>::equal(const xoptional<CTO, CBO>& rhs) const noexcept -> bool
    {
        return (!m_flag && !rhs.m_flag) || (m_value == rhs.m_value && (m_flag && rhs.m_flag));
    }

    template <class CT, class CB>
    template <class CTO>
    auto xoptional<CT, CB>::equal(const CTO& rhs) const noexcept -> disable_xoptional<CTO, bool>
    {
        return m_flag ? (m_value == rhs) : false;
    }

    template <class CT, class CB>
    inline auto xoptional<CT, CB>::operator&() & -> xclosure_pointer<self_type&>
    {
        return xclosure_pointer<self_type&>(*this);
    }

    template <class CT, class CB>
    inline auto xoptional<CT, CB>::operator&() const & -> xclosure_pointer<const self_type&>
    {
        return xclosure_pointer<const self_type&>(*this);
    }

    template <class CT, class CB>
    inline auto xoptional<CT, CB>::operator&() && -> xclosure_pointer<self_type>
    {
        return xclosure_pointer<self_type>(std::move(*this));
    }

    // External operators
    template <class T, class B, class OC, class OT>
    inline std::basic_ostream<OC, OT>& operator<<(std::basic_ostream<OC, OT>& out, const xoptional<T, B>& v)
    {
        if (v.has_value())
        {
            out << v.value();
        }
        else
        {
            out << "N/A";
        }
        return out;
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator==(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> bool
    {
        return e1.equal(e2);
    }

    template <class T1, class B1, class T2>
    inline auto operator==(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, bool>
    {
        return e1.equal(e2);
    }

    template <class T1, class T2, class B2>
    inline auto operator==(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, bool>
    {
        return e2.equal(e1);
    }

    template <class T, class B>
    inline auto operator+(const xoptional<T, B>& e) noexcept
        -> xoptional<std::decay_t<T>>
    {
        return e;
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator!=(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> bool
    {
        return !e1.equal(e2);
    }

    template <class T1, class B1, class T2>
    inline auto operator!=(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, bool>
    {
        return !e1.equal(e2);
    }

    template <class T1, class T2, class B2>
    inline auto operator!=(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, bool>
    {
        return !e2.equal(e1);
    }

    // Operations
    template <class T, class B>
    inline auto operator-(const xoptional<T, B>& e) noexcept
        -> xoptional<std::decay_t<T>>
    {
        using value_type = std::decay_t<T>;
        return e.has_value() ? -e.value() : missing<value_type>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator+(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() && e2.has_value() ? e1.value() + e2.value() : missing<value_type>();
    }

    template <class T1, class T2, class B2>
    inline auto operator+(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e2.has_value() ? e1 + e2.value() : missing<value_type>();
    }

    template <class T1, class B1, class T2>
    inline auto operator+(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() ? e1.value() + e2 : missing<value_type>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator-(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() && e2.has_value() ? e1.value() - e2.value() : missing<value_type>();
    }

    template <class T1, class T2, class B2>
    inline auto operator-(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e2.has_value() ? e1 - e2.value() : missing<value_type>();
    }

    template <class T1, class B1, class T2>
    inline auto operator-(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() ? e1.value() - e2 : missing<value_type>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator*(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() && e2.has_value() ? e1.value() * e2.value() : missing<value_type>();
    }

    template <class T1, class T2, class B2>
    inline auto operator*(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e2.has_value() ? e1 * e2.value() : missing<value_type>();
    }

    template <class T1, class B1, class T2>
    inline auto operator*(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() ? e1.value() * e2 : missing<value_type>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator/(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() && e2.has_value() ? e1.value() / e2.value() : missing<value_type>();
    }

    template <class T1, class T2, class B2>
    inline auto operator/(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e2.has_value() ? e1 / e2.value() : missing<value_type>();
    }

    template <class T1, class B1, class T2>
    inline auto operator/(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() ? e1.value() / e2 : missing<value_type>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator||(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() && e2.has_value() ? e1.value() || e2.value() : missing<value_type>();
    }

    template <class T1, class T2, class B2>
    inline auto operator||(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e2.has_value() ? e1 || e2.value() : missing<value_type>();
    }

    template <class T1, class B1, class T2>
    inline auto operator||(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() ? e1.value() || e2 : missing<value_type>();
    }


    template <class T1, class B1, class T2, class B2>
    inline auto operator&&(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() && e2.has_value() ? e1.value() && e2.value() : missing<value_type>();
    }

    template <class T1, class T2, class B2>
    inline auto operator&&(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> disable_xoptional<T1, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e2.has_value() ? e1 && e2.value() : missing<value_type>();
    }

    template <class T1, class B1, class T2>
    inline auto operator&&(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> disable_xoptional<T2, common_optional_t<T1, T2>>
    {
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;
        return e1.has_value() ? e1.value() && e2 : missing<value_type>();
    }

    template <class T, class B>
    inline auto operator!(const xoptional<T, B>& e) noexcept
        -> xoptional<bool>
    {
        return e.has_value() ? !e.value() : missing<bool>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator<(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() && e2.has_value() ? e1.value() < e2.value() : missing<bool>();
    }

    template <class T1, class T2, class B2>
    inline auto operator<(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e2.has_value() ? e1 < e2.value() : missing<bool>();
    }

    template <class T1, class B1, class T2>
    inline auto operator<(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() ? e1.value() < e2 : missing<bool>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator<=(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() && e2.has_value() ? e1.value() <= e2.value() : missing<bool>();
    }

    template <class T1, class T2, class B2>
    inline auto operator<=(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e2.has_value() ? e1 <= e2.value() : missing<bool>();
    }

    template <class T1, class B1, class T2>
    inline auto operator<=(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() ? e1.value() <= e2 : missing<bool>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator>(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() && e2.has_value() ? e1.value() > e2.value() : missing<bool>();
    }

    template <class T1, class T2, class B2>
    inline auto operator>(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e2.has_value() ? e1 > e2.value() : missing<bool>();
    }

    template <class T1, class B1, class T2>
    inline auto operator>(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() ? e1.value() > e2 : missing<bool>();
    }

    template <class T1, class B1, class T2, class B2>
    inline auto operator>=(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() && e2.has_value() ? e1.value() >= e2.value() : missing<bool>();
    }

    template <class T1, class T2, class B2>
    inline auto operator>=(const T1& e1, const xoptional<T2, B2>& e2) noexcept
        -> xoptional<bool>
    {
        return e2.has_value() ? e1 >= e2.value() : missing<bool>();
    }

    template <class T1, class B1, class T2>
    inline auto operator>=(const xoptional<T1, B1>& e1, const T2& e2) noexcept
        -> xoptional<bool>
    {
        return e1.has_value() ? e1.value() >= e2 : missing<bool>();
    }

#define UNARY_OPTIONAL(NAME)                                                 \
    template <class T, class B>                                              \
    inline auto NAME(const xoptional<T, B>& e)                               \
    {                                                                        \
        using std::NAME;                                                     \
        return e.has_value() ? NAME(e.value()) : missing<std::decay_t<T>>(); \
    }

#define UNARY_BOOL_OPTIONAL(NAME)                                       \
    template <class T, class B>                                         \
    inline xoptional<bool> NAME(const xoptional<T, B>& e)               \
    {                                                                   \
        using std::NAME;                                                \
        return e.has_value() ? bool(NAME(e.value())) : missing<bool>(); \
    }

#define BINARY_OPTIONAL_1(NAME)                                                    \
    template <class T1, class B1, class T2>                                        \
    inline auto NAME(const xoptional<T1, B1>& e1, const T2& e2)                    \
        -> disable_xoptional<T2, common_optional_t<T1, T2>>                        \
    {                                                                              \
        using std::NAME;                                                           \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>; \
        return e1.has_value() ? NAME(e1.value(), e2) : missing<value_type>();      \
    }


#define BINARY_OPTIONAL_2(NAME)                                                    \
    template <class T1, class T2, class B2>                                        \
    inline auto NAME(const T1& e1, const xoptional<T2, B2>& e2)                    \
        -> disable_xoptional<T1, common_optional_t<T1, T2>>                        \
    {                                                                              \
        using std::NAME;                                                           \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>; \
        return e2.has_value() ? NAME(e1, e2.value()) : missing<value_type>();      \
    }

#define BINARY_OPTIONAL_12(NAME)                                                                        \
    template <class T1, class B1, class T2, class B2>                                                   \
    inline auto NAME(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2)                          \
    {                                                                                                   \
        using std::NAME;                                                                                \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>;                      \
        return e1.has_value() && e2.has_value() ? NAME(e1.value(), e2.value()) : missing<value_type>(); \
    }

#define BINARY_OPTIONAL(NAME) \
    BINARY_OPTIONAL_1(NAME)   \
    BINARY_OPTIONAL_2(NAME)   \
    BINARY_OPTIONAL_12(NAME)

#define TERNARY_OPTIONAL_1(NAME)                                                                     \
    template <class T1, class B1, class T2, class T3>                                                \
    inline auto NAME(const xoptional<T1, B1>& e1, const T2& e2, const T3& e3)                        \
        -> disable_both_xoptional<T2, T3, common_optional_t<T1, T2, T3>>                             \
    {                                                                                                \
        using std::NAME;                                                                             \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>, std::decay_t<T3>>; \
        return e1.has_value() ? NAME(e1.value(), e2, e3) : missing<value_type>();                    \
    }

#define TERNARY_OPTIONAL_2(NAME)                                                                     \
    template <class T1, class T2, class B2, class T3>                                                \
    inline auto NAME(const T1& e1, const xoptional<T2, B2>& e2, const T3& e3)                        \
        -> disable_both_xoptional<T1, T3, common_optional_t<T1, T2, T3>>                             \
    {                                                                                                \
        using std::NAME;                                                                             \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>, std::decay_t<T3>>; \
        return e2.has_value() ? NAME(e1, e2.value(), e3) : missing<value_type>();                    \
    }

#define TERNARY_OPTIONAL_3(NAME)                                                                     \
    template <class T1, class T2, class T3, class B3>                                                \
    inline auto NAME(const T1& e1, const T2& e2, const xoptional<T3, B3>& e3)                        \
        -> disable_both_xoptional<T1, T2, common_optional_t<T1, T2, T3>>                             \
    {                                                                                                \
        using std::NAME;                                                                             \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>, std::decay_t<T3>>; \
        return e3.has_value() ? NAME(e1, e2, e3.value()) : missing<value_type>();                    \
    }

#define TERNARY_OPTIONAL_12(NAME)                                                                             \
    template <class T1, class B1, class T2, class B2, class T3>                                               \
    inline auto NAME(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2, const T3& e3)                  \
        -> disable_xoptional<T3, common_optional_t<T1, T2, T3>>                                               \
    {                                                                                                         \
        using std::NAME;                                                                                      \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>, std::decay_t<T3>>;          \
        return (e1.has_value() && e2.has_value()) ? NAME(e1.value(), e2.value(), e3) : missing<value_type>(); \
    }

#define TERNARY_OPTIONAL_13(NAME)                                                                             \
    template <class T1, class B1, class T2, class T3, class B3>                                               \
    inline auto NAME(const xoptional<T1, B1>& e1, const T2& e2, const xoptional<T3, B3>& e3)                  \
        -> disable_xoptional<T2, common_optional_t<T1, T2, T3>>                                               \
    {                                                                                                         \
        using std::NAME;                                                                                      \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>, std::decay_t<T3>>;          \
        return (e1.has_value() && e3.has_value()) ? NAME(e1.value(), e2, e3.value()) : missing<value_type>(); \
    }

#define TERNARY_OPTIONAL_23(NAME)                                                                             \
    template <class T1, class T2, class B2, class T3, class B3>                                               \
    inline auto NAME(const T1& e1, const xoptional<T2, B2>& e2, const xoptional<T3, B3>& e3)                  \
        -> disable_xoptional<T1, common_optional_t<T1, T2, T3>>                                               \
    {                                                                                                         \
        using std::NAME;                                                                                      \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>, std::decay_t<T3>>;          \
        return (e2.has_value() && e3.has_value()) ? NAME(e1, e2.value(), e3.value()) : missing<value_type>(); \
    }

#define TERNARY_OPTIONAL_123(NAME)                                                                                                      \
    template <class T1, class B1, class T2, class B2, class T3, class B3>                                                               \
    inline auto NAME(const xoptional<T1, B1>& e1, const xoptional<T2, B2>& e2, const xoptional<T3, B3>& e3)                             \
    {                                                                                                                                   \
        using std::NAME;                                                                                                                \
        using value_type = std::common_type_t<std::decay_t<T1>, std::decay_t<T2>, std::decay_t<T3>>;                                    \
        return (e1.has_value() && e2.has_value() && e3.has_value()) ? NAME(e1.value(), e2.value(), e3.value()) : missing<value_type>(); \
    }

#define TERNARY_OPTIONAL(NAME) \
    TERNARY_OPTIONAL_1(NAME)   \
    TERNARY_OPTIONAL_2(NAME)   \
    TERNARY_OPTIONAL_3(NAME)   \
    TERNARY_OPTIONAL_12(NAME)  \
    TERNARY_OPTIONAL_13(NAME)  \
    TERNARY_OPTIONAL_23(NAME)  \
    TERNARY_OPTIONAL_123(NAME)

    UNARY_OPTIONAL(abs)
    UNARY_OPTIONAL(fabs)
    BINARY_OPTIONAL(fmod)
    BINARY_OPTIONAL(remainder)
    TERNARY_OPTIONAL(fma)
    BINARY_OPTIONAL(fmax)
    BINARY_OPTIONAL(fmin)
    BINARY_OPTIONAL(fdim)
    UNARY_OPTIONAL(exp)
    UNARY_OPTIONAL(exp2)
    UNARY_OPTIONAL(expm1)
    UNARY_OPTIONAL(log)
    UNARY_OPTIONAL(log10)
    UNARY_OPTIONAL(log2)
    UNARY_OPTIONAL(log1p)
    BINARY_OPTIONAL(pow)
    UNARY_OPTIONAL(sqrt)
    UNARY_OPTIONAL(cbrt)
    BINARY_OPTIONAL(hypot)
    UNARY_OPTIONAL(sin)
    UNARY_OPTIONAL(cos)
    UNARY_OPTIONAL(tan)
    UNARY_OPTIONAL(acos)
    UNARY_OPTIONAL(asin)
    UNARY_OPTIONAL(atan)
    BINARY_OPTIONAL(atan2)
    UNARY_OPTIONAL(sinh)
    UNARY_OPTIONAL(cosh)
    UNARY_OPTIONAL(tanh)
    UNARY_OPTIONAL(acosh)
    UNARY_OPTIONAL(asinh)
    UNARY_OPTIONAL(atanh)
    UNARY_OPTIONAL(erf)
    UNARY_OPTIONAL(erfc)
    UNARY_OPTIONAL(tgamma)
    UNARY_OPTIONAL(lgamma)
    UNARY_BOOL_OPTIONAL(isfinite)
    UNARY_BOOL_OPTIONAL(isinf)
    UNARY_BOOL_OPTIONAL(isnan)

#undef TERNARY_OPTIONAL
#undef TERNARY_OPTIONAL_123
#undef TERNARY_OPTIONAL_23
#undef TERNARY_OPTIONAL_13
#undef TERNARY_OPTIONAL_12
#undef TERNARY_OPTIONAL_3
#undef TERNARY_OPTIONAL_2
#undef TERNARY_OPTIONAL_1
#undef BINARY_OPTIONAL
#undef BINARY_OPTIONAL_12
#undef BINARY_OPTIONAL_2
#undef BINARY_OPTIONAL_1
#undef UNARY_OPTIONAL
}

#endif
