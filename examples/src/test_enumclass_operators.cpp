#include <ncr/ncr_utils.hpp>
#include <iostream>

#define USE_NCR_MACRO

// 2023-09-27: I just discovered https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html
//             and wondered if it might make sense to incorporate the approach
//             via template into ncr. However, looking at it, there is no
//             significant difference beside ncr using a simple macro and Just
//             Software's template instantiation. There might be some benefits
//             in having the template, but for now stick to the macro.
//
enum class Foo {
	One,
	Two,
	Three
};

#ifdef USE_NCR_MACRO
	NCR_DEFINE_ENUM_FLAG_OPERATORS(Foo);
#else

/*
 * enable bitmask operators using templates
 */
template<typename E>
struct enable_bitmask_operators{
	static constexpr bool enable=false;
};


template<typename E>
typename std::enable_if<enable_bitmask_operators<E>::enable,E>::type
operator|(E lhs,E rhs)
{
	typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename E>
typename std::enable_if<enable_bitmask_operators<E>::enable,E>::type
operator&(E lhs,E rhs)
{
	typedef typename std::underlying_type<E>::type underlying;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<>
struct enable_bitmask_operators<Foo>
{
	static constexpr bool enable=true;
};

#endif



void bar(Foo f)
{
	if ((f & Foo::One) == Foo::One) {
		std::cout << "foo is one\n";
	}
	if ((f & Foo::Two) == Foo::Two) {
		std::cout << "foo is two\n";
	}
	if ((f & Foo::Two) == Foo::Three) {
		std::cout << "foo is three\n";
	}
}


int main()
{
	bar (Foo::One | Foo::Two);
	return 0;
}
