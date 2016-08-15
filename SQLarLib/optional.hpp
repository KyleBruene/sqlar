/**
 *	\file
 */
 
 
#pragma once


#include <exception>
#include <functional>
#include <new>
#include <initializer_list>
#include <type_traits>
#include <utility>
 
 
namespace std {


	class nullopt_t {
	
	
		public:
		
		
			constexpr nullopt_t () noexcept {	};
	
	
	};
	
	
	constexpr nullopt_t nullopt;
	
	
	class in_place_t {
	
	
		public:
		
		
			constexpr in_place_t () noexcept {	};
	
	
	};
	
	
	constexpr in_place_t in_place;
	
	
	class bad_optional_access : public exception {	};


	template <typename T>
	class optional {
	
	
		private:
		
		
			static constexpr bool nothrow_copy=std::is_nothrow_copy_constructible<T>::value;
			static constexpr bool nothrow_move=std::is_nothrow_move_constructible<T>::value;
			static constexpr bool nothrow_copy_assign=std::is_nothrow_copy_assignable<T>::value && nothrow_copy;
			static constexpr bool nothrow_move_assign=std::is_nothrow_move_assignable<T>::value && nothrow_move;
		
		
			bool engaged;
			typename aligned_storage<
				sizeof(T),
				alignof(T)
			>::type storage;
			
			
			T * get_ptr () noexcept {
			
				return reinterpret_cast<T *>(&storage);
			
			}
			
			
			constexpr const T * get_ptr () const noexcept {
			
				return reinterpret_cast<const T *>(&storage);
			
			}
			
			
			T & get () noexcept {
			
				return *get_ptr();
			
			}
			
			
			constexpr const T & get () const noexcept {
			
				return *get_ptr();
			
			}
			
			
			void destroy () noexcept {
			
				if (!engaged) return;
				
				get().~T();
				engaged=false;
			
			}
			
			
			template <typename... Args>
			/*constexpr*/ void construct (Args &&... args) noexcept(std::is_nothrow_constructible<T,Args...>::value) {
			
				new (get_ptr()) T (std::forward<Args>(args)...);
			
			}
	
	
		public:
		
		
			static constexpr bool nothrow_equal=noexcept(std::declval<const T &>()==std::declval<const T &>());
			static constexpr bool nothrow_less=noexcept(std::declval<const T &>()<std::declval<const T &>());
		
		
			typedef T value_type;
			
			
			constexpr optional () noexcept : engaged(false) {	}
			
			
			constexpr optional (nullopt_t) noexcept : engaged(false) {	}
			
			
			optional (const optional & other) noexcept(nothrow_copy) : engaged(other.engaged) {
			
				if (engaged) construct(other.get());
			
			}
			
			
			optional (optional && other) noexcept(nothrow_move) : engaged(other.engaged) {
			
				if (engaged) construct(std::move(other.get()));
			
			}
			
			
			/*constexpr*/ optional (const T & value) noexcept(nothrow_copy) : engaged(true) {
			
				construct(value);
			
			}
			
			
			/*constexpr*/ optional (T && value) noexcept(nothrow_move) : engaged(true) {
			
				construct(std::move(value));
			
			}
			
			
			template <typename... Args>
			/*constexpr*/ explicit optional (std::in_place_t, Args &&... args) noexcept(std::is_nothrow_constructible<T,Args...>::value) : engaged(true) {
			
				construct(std::forward<Args>(args)...);
			
			}
			
			
			template <typename U, typename... Args>
			/*constexpr*/ explicit optional (std::in_place_t, std::initializer_list<U> ilist, Args &&... args) noexcept(
				std::is_nothrow_constructible<
					T,
					std::initializer_list<U>,
					Args...
				>::value
			) : engaged(true) {
			
				construct(std::move(ilist),std::forward<Args>(args)...);
			
			}
			
			
			~optional () noexcept {
			
				destroy();
			
			}
			
			
			optional & operator = (std::nullopt_t) noexcept {
			
				destroy();
				
				return *this;
			
			}
			
			
			optional & operator = (const optional & other) noexcept(nothrow_copy_assign) {
			
				if (other.engaged) {
				
					if (engaged) {
					
						get()=other.get();
					
					} else {
					
						construct(other.get());
						engaged=true;
					
					}
				
				} else {
				
					destroy();
				
				}
				
				return *this;
			
			}
			
			
			optional & operator = (optional && other) noexcept(nothrow_move_assign) {
			
				if (other.engaged) {
				
					if (engaged) {
					
						get()=std::move(other.get());
					
					} else {
					
						construct(std::move(other.get()));
						engaged=true;
					
					}
				
				} else {
				
					destroy();
				
				}
				
				return *this;
			
			}
			
			
			template <typename U>
			typename std::enable_if<
				!std::is_same<
					typename std::decay<U>::type,
					optional
				>::value,
				optional &
			>::type operator = (U && value) noexcept(
				std::is_nothrow_constructible<T,U>::value &&
				std::is_nothrow_assignable<T,U>::value
			) {
			
				if (engaged) {
				
					get()=std::forward<U>(value);
				
				} else {
				
					construct(std::forward<U>(value));
					engaged=true;
				
				}
				
				return *this;
			
			}
			
			
			constexpr const T * operator -> () const noexcept {
			
				return get_ptr();
			
			}
			
			
			T * operator -> () noexcept {
			
				return get_ptr();
			
			}
			
			
			constexpr const T & operator * () const noexcept {
			
				return get();
			
			}
			
			
			T & operator * () noexcept {
			
				return get();
			
			}
			
			
			constexpr explicit operator bool () const noexcept {
			
				return engaged;
			
			}
			
			
			constexpr const T & value () const {
			
				if (engaged) return get();
				
				throw bad_optional_access{};
			
			}
			
			
			T & value () {
			
				if (engaged) return get();
				
				throw bad_optional_access{};
			
			}
			
			
			template <typename U>
			constexpr T value_or (U && value) const & noexcept(
				nothrow_copy &&
				std::is_nothrow_constructible<T,U>::value
			) {
				
				if (engaged) return get();
				
				return value;
			
			}
			
			
			template <typename U>
			T value_or (U && value) && noexcept(
				nothrow_move &&
				std::is_nothrow_constructible<T,U>::value
			) {
			
				if (engaged) return std::move(get());
				
				return value;
			
			}
			
			
			void swap (optional & other) noexcept(
				nothrow_move &&
				noexcept(swap(declval<T &>(),declval<T &>()))
			) {
			
				if (engaged) {
				
					if (other.engaged) {
					
						std::swap(get(),other.get());
					
					} else {
					
						other.construct(std::move(get()));
						destroy();
					
					}
				
				} else if (other.engaged) {
				
					construct(std::move(other.get()));
					other.destroy();
				
				}
			
			}
			
			
			template <typename... Args>
			void emplace (Args &&... args) noexcept(std::is_nothrow_constructible<T,Args...>::value) {
			
				destroy();
				
				construct(std::forward<Args>(args)...);
				engaged=true;
			
			}
			
			
			template <typename U, typename... Args>
			void emplace (std::initializer_list<U> ilist, Args &&... args) noexcept(
				std::is_nothrow_constructible<
					T,
					std::initializer_list<U>,
					Args...
				>::value
			) {
			
				destroy();
				
				construct(std::move(ilist),std::forward<Args>(args)...);
				engaged=true;
			
			}
	
	
	};
	
	
	template <typename T>
	constexpr bool operator == (const optional<T> & lhs, const optional<T> & rhs) noexcept(optional<T>::nothrow_equal) {
	
		return (lhs && rhs) ? (*lhs==*rhs) : !(lhs || rhs);
	
	}
	
	
	template <typename T>
	constexpr bool operator < (const optional<T> & lhs, const optional<T> & rhs) noexcept(optional<T>::nothrow_less) {
	
		return (lhs && rhs) ? (*lhs<*rhs) : (!lhs && rhs);
	
	}
	
	
	template <typename T>
	constexpr bool operator == (const optional<T> & opt, std::nullopt_t) noexcept {
	
		return !opt;
	
	}
	
	
	template <typename T>
	constexpr bool operator == (std::nullopt_t, const optional<T> & opt) noexcept {
	
		return !opt;
	
	}
	
	
	template <typename T>
	constexpr bool operator < (const optional<T> & opt, std::nullopt_t) noexcept {
	
		return !!opt;
	
	}
	
	
	template <typename T>
	constexpr bool operator < (std::nullopt_t, const optional<T> & opt) noexcept {
	
		return false;
	
	}
	
	
	template <typename T>
	constexpr bool operator == (const optional<T> & opt, const T & v) noexcept(optional<T>::nothrow_equal) {
	
		return opt ? (*opt==v) : false;
	
	}
	
	
	template <typename T>
	constexpr bool operator == (const T & value, const optional<T> & opt) noexcept(optional<T>::nothrow_equal) {
	
		return opt==value;
	
	}
	
	
	template <typename T>
	constexpr bool operator < (const optional<T> & opt, const T & v) noexcept(optional<T>::nothrow_less) {
	
		return opt ? (*opt<v) : true;
	
	}
	
	
	template <typename T>
	constexpr optional<typename std::decay<T>::type> make_optional (T && value) noexcept(
		std::is_nothrow_constructible<
			typename std::decay<T>::type,
			T
		>::value &&
		std::is_nothrow_move_constructible<
			optional<typename std::decay<T>::type>
		>::value
	) {
	
		return optional<typename std::decay<T>::type>(std::forward<T>(value));
	
	}
	
	
	template <typename T>
	void swap (optional<T> & lhs, optional<T> & rhs) noexcept(noexcept(lhs.swap(rhs))) {
	
		lhs.swap(rhs);
	
	}
	
	
	template <typename T>
	struct hash<optional<T>> {
	
	
		private:
		
		
			typedef typename std::decay<T>::type type;
		
		
			constexpr static bool nothrow=std::is_nothrow_constructible<
				hash<type>
			>::value && noexcept(
				declval<hash<type> &>()(declval<const type &>())
			);
			
			
		public:
		
		
			size_t operator () (const optional<T> & obj) const noexcept(nothrow) {
			
				return obj ? hash<type>{}(*obj) : 0;
			
			}
	
	
	};


}
 
