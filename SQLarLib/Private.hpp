
//A Smart Pointer to IMPLementation (i.e. Smart PIMPL or just SPIMPL).
//********************************************************************
//Copyright (c) 2015 Andrey Upadyshev (oliora@gmail.com)
//Distributed under the Boost Software License, Version 1.0.
//See http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <memory>
#include <type_traits>
#include <cassert>

namespace spimpl
{
namespace details 
{
	template<class T>
	T *default_copy(T *src)
	{
		static_assert(sizeof(T) > 0, "default_copy cannot copy incomplete type");
		static_assert(!std::is_void<T>::value, "default_copy cannot copy incomplete type");
		return new T(*src);
	}

	template<class T>
	void default_delete(T *p) noexcept
	{
		static_assert(sizeof(T) > 0, "default_delete cannot delete incomplete type");
		static_assert(!std::is_void<T>::value, "default_delete cannot delete incomplete type");
		delete p;
	}

	template<class T>
	struct default_deleter 
	{
		using type = void(*)(T*);
	};

	template<class T>
	using default_deleter_t = typename default_deleter<T>::type;

	template<class T>
	struct default_copier 
	{
		using type = T* (*)(T*);
	};

	template<class T>
	using default_copier_t = typename default_copier<T>::type;

	template<class T, class D, class C>
	struct is_default_manageable : public std::integral_constant<bool,
		std::is_same<D, default_deleter_t<T>>::value &&
		std::is_same<C, default_copier_t<T>>::value> 
	{ };
}


template<class T, class Deleter = details::default_deleter_t<T>, class Copier = details::default_copier_t<T>>
class impl_ptr
{
private:
	static_assert(!std::is_array<T>::value, "impl_ptr specialization for arrays is not implemented");
	struct dummy_t_ { int dummy__; };

public:
	using pointer = T*;
	using element_type = T;
	using copier_type = typename std::decay<Copier>::type;
	using deleter_type = typename std::decay<Deleter>::type;
	using unique_ptr_type = std::unique_ptr<T, deleter_type>;
	using is_default_manageable = details::is_default_manageable<T, deleter_type, copier_type>;

	constexpr impl_ptr() noexcept
		: ptr_(nullptr, deleter_type{}), copier_(copier_type{}) {}

	constexpr impl_ptr(std::nullptr_t) noexcept
		: impl_ptr() {}

	template<class D, class C>
	impl_ptr(pointer p, D&& d, C&& c,
		typename std::enable_if<
			std::is_convertible<D, deleter_type>::value
			&& std::is_convertible<C, copier_type>::value,
			dummy_t_>::type = dummy_t_()) noexcept
		: ptr_(std::move(p), std::forward<D>(d)), copier_(std::forward<C>(c)) 
	{ }

	template<class U>
	impl_ptr(U *u,
		typename std::enable_if<
			std::is_convertible<U*, pointer>::value
			&& is_default_manageable::value,
			dummy_t_>::type = dummy_t_()) noexcept
		: impl_ptr(u, &details::default_delete<T>, &details::default_copy<T>) {}

	impl_ptr(const impl_ptr& r)
		: impl_ptr(r.clone()) 
	{ }

	impl_ptr(impl_ptr&& r) noexcept = default;

	template<class U>
	impl_ptr(std::auto_ptr<U>&& u,
		typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& is_default_manageable::value,
		dummy_t_>::type = dummy_t_()) noexcept
		: ptr_(u.release(), &details::default_delete<T>), copier_(&details::default_copy<T>) {}

	template<class U>
	impl_ptr(std::unique_ptr<U>&& u,
		typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& is_default_manageable::value,
		dummy_t_>::type = dummy_t_()) noexcept
		: ptr_(u.release(), &details::default_delete<T>), copier_(&details::default_copy<T>) {}

	template<class U, class D, class C>
	impl_ptr(std::unique_ptr<U, D>&& u, C&& c,
		typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& std::is_convertible<D, deleter_type>::value
		&& std::is_convertible<C, copier_type>::value,
		dummy_t_>::type = dummy_t_()) noexcept
		: ptr_(std::move(u)), copier_(std::forward<C>(c)) {}

	template<class U, class D, class C>
	impl_ptr(impl_ptr<U, D, C>&& u,
		typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& std::is_convertible<D, deleter_type>::value
		&& std::is_convertible<C, copier_type>::value,
		dummy_t_>::type = dummy_t_()) noexcept
		: ptr_(std::move(u.ptr_)), copier_(std::move(u.copier_)) {}

	impl_ptr& operator= (const impl_ptr& r)
	{
		if (this == &r)
			return *this;

		return operator=(r.clone());
	}

	impl_ptr& operator= (impl_ptr&& r) noexcept = default;

	template<class U, class D, class C>
	typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& std::is_convertible<D, deleter_type>::value
		&& std::is_convertible<C, copier_type>::value,
		impl_ptr&>::type operator= (impl_ptr<U, D, C>&& u) noexcept
	{
		ptr_ = std::move(u.ptr_);
		copier_ = std::move(u.copier_);
		return *this;
	}

	template<class U, class D, class C>
	typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& std::is_convertible<D, deleter_type>::value
		&& std::is_convertible<C, copier_type>::value,
		impl_ptr&>::type operator= (const impl_ptr<U, D, C>& u)
	{
		return operator=(u.clone());
	}

	template<class U>
	typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& is_default_manageable::value,
		impl_ptr&>::type operator= (std::auto_ptr<U>&& u) noexcept
	{
		return operator=(impl_ptr(std::move(u)));
	}

	template<class U>
	typename std::enable_if<
		std::is_convertible<U*, pointer>::value
		&& is_default_manageable::value,
		impl_ptr&>::type operator= (std::unique_ptr<U>&& u) noexcept
	{
		return operator=(impl_ptr(std::move(u)));
	}

	impl_ptr clone() const
	{
		return impl_ptr(
			ptr_ ? copier_(ptr_.get()) : nullptr,
			ptr_.get_deleter(),
			copier_);
	}

	typename std::remove_reference<T>::type & operator*() const { return *ptr_; }
	pointer operator->() const noexcept { return get(); }
	pointer get() const noexcept { return ptr_.get(); }

	void swap(impl_ptr& u) noexcept
	{
		using std::swap;
		ptr_.swap(u.ptr_);
		swap(copier_, u.copier_);
	}

	pointer release() noexcept { return ptr_.release(); }

	unique_ptr_type release_unique() noexcept { return std::move(ptr_); }

	explicit operator bool() const noexcept { return static_cast<bool>(ptr_); }

	typename std::remove_reference<deleter_type>::type& get_deleter() noexcept { return ptr_.get_deleter(); }
	const typename std::remove_reference<deleter_type>::type& get_deleter() const noexcept { return ptr_.get_deleter(); }

	typename std::remove_reference<copier_type>::type& get_copier() noexcept { return copier_; }
	const typename std::remove_reference<copier_type>::type& get_copier() const noexcept { return copier_; }

	//T& operator* ()
	//{
	//	return *(ptr_.get());
	//}

	//T* operator-> ()
	//{
	//	return ptr_.get();
	//}

private:
	// TODO: use compressed_pair<unique_ptr_type, copier_type> instead
	unique_ptr_type ptr_;
	copier_type copier_;
};

template<class T, class D, class C>
inline void swap(impl_ptr<T, D, C>& l, impl_ptr<T, D, C>& r) noexcept
{
	l.swap(r);
}


template <class T1, class D1, class C1, class T2, class D2, class C2>
inline bool operator==(const impl_ptr<T1, D1, C1>& l, const impl_ptr<T2, D2, C2>& r)
{
	return l.get() == r.get();
}

template <class T1, class D1, class C1, class T2, class D2, class C2>
inline bool operator!=(const impl_ptr<T1, D1, C1>& l, const impl_ptr<T2, D2, C2>& r)
{
	return !(l == r);
}

template <class T1, class D1, class C1, class T2, class D2, class C2>
inline bool operator< (const impl_ptr<T1, D1, C1>& l, const impl_ptr<T2, D2, C2>& r)
{
	using P1 = typename impl_ptr<T1, D1, C1>::pointer;
	using P2 = typename impl_ptr<T2, D2, C2>::pointer;
	using CT = typename std::common_type<P1, P2>::type;
	return std::less<CT>()(l.get(), r.get());
}

template <class T1, class D1, class C1, class T2, class D2, class C2>
inline bool operator> (const impl_ptr<T1, D1, C1>& l, const impl_ptr<T2, D2, C2>& r)
{
	return r < l;
}

template <class T1, class D1, class C1, class T2, class D2, class C2>
inline bool operator<=(const impl_ptr<T1, D1, C1>& l, const impl_ptr<T2, D2, C2>& r)
{
	return !(r < l);
}

template <class T1, class D1, class C1, class T2, class D2, class C2>
inline bool operator>=(const impl_ptr<T1, D1, C1>& l, const impl_ptr<T2, D2, C2>& r)
{
	return !(l < r);
}

template <class T, class D, class C>
inline bool operator==(const impl_ptr<T, D, C>& p, std::nullptr_t) noexcept
{
	return !p;
}

template <class T, class D, class C>
inline bool operator==(std::nullptr_t, const impl_ptr<T, D, C>& p) noexcept
{
	return !p;
}

template <class T, class D, class C>
inline bool operator!=(const impl_ptr<T, D, C>& p, std::nullptr_t) noexcept
{
	return static_cast<bool>(p);
}

template <class T, class D, class C>
inline bool operator!=(std::nullptr_t, const impl_ptr<T, D, C>& p) noexcept
{
	return static_cast<bool>(p);
}

template <class T, class D, class C>
inline bool operator< (const impl_ptr<T, D, C>& l, std::nullptr_t)
{
	using P = typename impl_ptr<T, D, C>::pointer;
	return std::less<P>()(l.get(), nullptr);
}

template <class T, class D, class C>
inline bool operator< (std::nullptr_t, const impl_ptr<T, D, C>& p)
{
	using P = typename impl_ptr<T, D, C>::pointer;
	return std::less<P>()(nullptr, p.get());
}

template <class T, class D, class C>
inline bool operator> (const impl_ptr<T, D, C>& p, std::nullptr_t)
{
	return nullptr < p;
}

template <class T, class D, class C>
inline bool operator> (std::nullptr_t, const impl_ptr<T, D, C>& p)
{
	return p < nullptr;
}

template <class T, class D, class C>
inline bool operator<=(const impl_ptr<T, D, C>& p, std::nullptr_t)
{
	return !(nullptr < p);
}

template <class T, class D, class C>
inline bool operator<=(std::nullptr_t, const impl_ptr<T, D, C>& p)
{
	return !(p < nullptr);
}

template <class T, class D, class C>
inline bool operator>=(const impl_ptr<T, D, C>& p, std::nullptr_t)
{
	return !(p < nullptr);
}

template <class T, class D, class C>
inline bool operator>=(std::nullptr_t, const impl_ptr<T, D, C>& p)
{
	return !(nullptr < p);
}


template<class T, class... Args>
inline impl_ptr<T> make_impl(Args&&... args)
{
	return impl_ptr<T>(new T(std::forward<Args>(args)...), &details::default_delete<T>, &details::default_copy<T>);
}


// Helpers to manage unique impl, stored in std::unique_ptr

template<class T, class Deleter = void(*)(T*)>
using unique_impl_ptr = std::unique_ptr<T, Deleter>;

template<class T, class... Args>
inline unique_impl_ptr<T> make_impl_nocopy(Args&&... args)
{
	static_assert(!std::is_array<T>::value, "unique_impl_ptr does not support arrays");
	return unique_impl_ptr<T>(new T(std::forward<Args>(args)...), &details::default_delete<T>);
}
}

namespace std
{
	template <class T, class D, class C>
	struct hash<spimpl::impl_ptr<T, D, C>>
	{
		using argument_type = spimpl::impl_ptr<T, D, C>;
		using result_type = size_t;

		result_type operator()(const argument_type& p) const noexcept
		{
			return hash<typename argument_type::pointer>()(p.get());
		}
	};
}








//********************************************************************
//********************************************************************
//********************************************************************
//********************************************************************
//Copyright (c) 2015 Aiden Koss
//Copyright (c) 2015 Open Systems International, Inc
//Distributed under the Boost Software License, Version 1.0.
//See http://www.boost.org/LICENSE_1_0.txt

using namespace spimpl;

#define ADDPRIVATE_STRUCT_COPYABLE \
private:                       \
	struct Private;            \
	friend Private;            \
	impl_ptr<Private> impl;    \
public:                        \
	static bool isCopyable() {return true;}

#define ADDPRIVATE_CLASS_COPYABLE \
private:                          \
	struct Private;               \
	friend Private;               \
	impl_ptr<Private> impl;       \
public:                           \
	static bool IsCopyable() {return true;} \
private:


#define ADDPRIVATE_STRUCT_NOCOPY   \
private:                           \
	struct Private;                \
	friend Private;                \
	unique_impl_ptr<Private> impl; \
public:                            \
	static bool IsCopyable() {return false;}

#define ADDPRIVATE_CLASS_NOCOPY    \
private:                           \
	struct Private;                \
	friend Private;                \
	unique_impl_ptr<Private> impl; \
public:                            \
	static bool IsCopyable() {return false;} \
private:




