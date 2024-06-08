#include <iostream>
#include <ncr/ncr_algorithm.hpp>
#include <ncr/ncr_bitset.hpp>

#include <ranges>
#include <stdexcept>

// compile time fixed size bitset
void
test_bitset()
{
	using namespace std;

	cout << "ncr::bitset - Compile Time Fixed Size bitset\n";
	// default test
	{
		cout << "basic test\n";
		ncr::bitset<32, uint8_t> bits;

		cout << bits.size() << ", " << bits.word_count() << "x" << bits.bits_per_word() << " bits" << "\n";

		for (size_t i = 0; i < 32; i++) {
			if (i < 16) {
				bits.set(i);
				bits.set(32 - i - 1);
			}
			std::string s = bits.to_string('.');
			cout << s << "\n";
			bits.reset(i);
		}

		cout << "testing any, all, none\n";
		bits.set();
		cout << bits << ": " << boolalpha << bits.any() << ", " << bits.all() << ", " << bits.none() << "\n" << noboolalpha;
		bits.reset(0);
		cout << bits << ": " << boolalpha << bits.any() << ", " << bits.all() << ", " << bits.none() << "\n" << noboolalpha;
		bits.reset();
		cout << bits << ": " << boolalpha << bits.any() << ", " << bits.all() << ", " << bits.none() << "\n" << noboolalpha;
	}

	// iterator stuff
	{
		cout << "\niterator test\n";

		ncr::bitset<16, uint8_t> bits;
		bits.set();
		bits.reset(10);
		bits.reset(11);

		cout << "ostream conversion " << bits << "\n";

		cout << "forward iterator   ";
		for (auto b : bits) {
			cout << b;
		}
		cout << "\n";

		cout << "reverse iterator   ";
		for (auto it = bits.rbegin(); it != bits.rend(); ++it) {
		// for (auto b : bits | std::views::reverse)
			cout << *it;
		}
		cout << "\n";
	}

	// hamming stuff and popcount
	{
		cout << "\nhamming distances, popcount\n";

		ncr::bitset<32, uint8_t> bits, bits2;

		bits.set();
		bits2.reset();

		cout << bits << ", count = " << bits.count() << "\n";
		cout << bits2 << ", count = " << bits2.count() << "\n";

		// hamming distance with specialized functions
		cout << "hamming = " << ncr::hamming(bits, bits2) << "\n";
		bits.reset(10);
		bits.reset(11);
		bits.reset(12);
		bits.reset(13);
		cout << bits << ", count = " << bits.count() << "\n";
		cout << bits2 << ", count = " << bits2.count() << "\n";
		cout << "hamming = " << ncr::hamming(bits, bits2) << "\n";

		// hamming distance with generic iterator based functions

		// levensthein distance stuff
		bits.set();
		bits2.set();

		bits.reset(11);
		bits2.reset(12);

		cout << bits << ", count = " << bits.count() << "\n";
		cout << bits2 << ", count = " << bits2.count() << "\n";

		size_t lv = ncr::levensthein(bits.rbegin(), bits.rend(), bits2.rbegin(), bits2.rend());
		cout << "levenstein = " << lv << "\n";

		std::string s1 = bits.to_string();
		std::string s2 = bits2.to_string();
		lv = ncr::levensthein(s1, s2);
		cout << "levenstein (ref) = " << lv << "\n";

	}

	// from initializer list test
	{
		cout << "\ninitializer list test\n";
		ncr::bitset<11, uint8_t> bits{0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1};
		std::string s = bits.to_string('.');
		cout << s << "\n";
		cout << int(bits._bits[0]) << ", " << int(bits._bits[1]) << "\n";
	}

	// from string test
	{
		cout << "\nfrom_string test\n";
		ncr::bitset<11, uint8_t> bits("01010100111");
		std::string s = bits.to_string('.');
		cout << s << "\n";
		cout << int(bits._bits[0]) << ", " << int(bits._bits[1]) << "\n";
	}

	// from vector test
	{
		cout << "\nfrom_vector test\n";
		ncr::bitset<16, uint8_t> bits;
		bits.from_vector({2, 0});
		std::string s = bits.to_string('.');
		cout << s << "\n";
		cout << bits << "\n";
	}

	// copy and move constructor test
	{
		cout << "\ncopy and move constructor test\n";
		ncr::bitset<11, uint8_t> bits("01010100111");
		cout << bits << "\n";

		// calls the copy constructor
		ncr::bitset<11, uint8_t> bits2 = bits;
		cout << bits2 << "\n";
		bits.reset(1);
		cout << bits << "\n";
		cout << bits2 << "\n";

		ncr::bitset<11, uint8_t> bits3(std::move(bits));
		cout << bits3 << "\n";
	}
}


// run-time variable size bitset
void
test_dynamic_bitset()
{
	using namespace std;

	cout << "ncr::dynamic_bitset - Run-Time Variable Size bitset\n";

	// default test
	{
		cout << "basic test\n";
		ncr::dynamic_bitset<uint8_t> bits;

		// resizing
		bits.resize(32);
		cout << bits.size() << ", " << bits.word_count() << "x" << bits.bits_per_word() << " bits" << "\n";

		for (size_t i = 0; i < 32; i++) {
			if (i < 16) {
				bits.set(i);
				bits.set(32 - i - 1);
			}
			std::string s = bits.to_string('.');
			cout << s << "\n";
			bits.reset(i);
		}

		cout << "testing any, all, none\n";
		bits.set();
		cout << bits << ": " << boolalpha << bits.any() << ", " << bits.all() << ", " << bits.none() << "\n" << noboolalpha;
		bits.reset(0);
		cout << bits << ": " << boolalpha << bits.any() << ", " << bits.all() << ", " << bits.none() << "\n" << noboolalpha;
		bits.reset();
		cout << bits << ": " << boolalpha << bits.any() << ", " << bits.all() << ", " << bits.none() << "\n" << noboolalpha;
	}

	// iterator stuff
	{
		cout << "\niterator test\n";

		ncr::dynamic_bitset<uint8_t> bits(2);
		bits.set();
		bits.reset(10);
		bits.reset(11);

		cout << "ostream conversion " << bits << "\n";

		cout << "forward iterator   ";
		for (auto b : bits) {
			cout << b;
		}
		cout << "\n";

		cout << "reverse iterator   ";
		for (auto it = bits.rbegin(); it != bits.rend(); ++it) {
		// for (auto b : bits | std::views::reverse)
			cout << *it;
		}
		cout << "\n";
	}

	// hamming stuff and popcount
	{
		cout << "\nhamming distances, popcount\n";

		ncr::dynamic_bitset<uint8_t> bits(4), bits2(4);

		bits.set();
		bits2.reset();

		cout << bits << ", count = " << bits.count() << "\n";
		cout << bits2 << ", count = " << bits2.count() << "\n";

		// hamming distance with specialized functions
		cout << "hamming = " << ncr::hamming(bits, bits2) << "\n";
		bits.reset(10);
		bits.reset(11);
		bits.reset(12);
		bits.reset(13);
		cout << bits << ", count = " << bits.count() << "\n";
		cout << bits2 << ", count = " << bits2.count() << "\n";
		cout << "hamming = " << ncr::hamming(bits, bits2) << "\n";

		// hamming distance with generic iterator based functions

		// levensthein distance stuff
		bits.set();
		bits2.set();

		bits.reset(11);
		bits2.reset(12);

		cout << bits << ", count = " << bits.count() << "\n";
		cout << bits2 << ", count = " << bits2.count() << "\n";

		size_t lv = ncr::levensthein(bits.rbegin(), bits.rend(), bits2.rbegin(), bits2.rend());
		cout << "levenstein = " << lv << "\n";

		std::string s1 = bits.to_string();
		std::string s2 = bits2.to_string();
		lv = ncr::levensthein(s1, s2);
		cout << "levenstein (ref) = " << lv << "\n";

	}

	// from initializer list test
	{
		cout << "\ninitializer list test\n";
		ncr::dynamic_bitset<uint8_t> bits{0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1};
		std::string s = bits.to_string('.');
		cout << s << " (length = " << bits.length() << ")\n";
		cout << int(bits._bits[0]) << ", " << int(bits._bits[1]) << "\n";

		// it is possible to pass how may padding bits there should be, in
		// case the initializer list doesn't cover the entire data. In the
		// example below, use 5 padding bits in addition to the first 11 ones
		// that are explicitely set.
		ncr::dynamic_bitset<uint8_t> bits2({0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1}, 5);
		s = bits2.to_string('.');
		cout << s << " (length = " << bits2.length() << ")\n";
		cout << int(bits._bits[0]) << ", " << int(bits._bits[1]) << "\n";

	}

	// from string test
	{
		cout << "\nfrom_string test\n";
		ncr::dynamic_bitset<uint8_t> bits("01010100111");
		std::string s = bits.to_string('.');
		cout << s << "\n";
		cout << int(bits._bits[0]) << ", " << int(bits._bits[1]) << "\n";
	}

	// from vector test
	{
		cout << "\nfrom_vector test\n";
		ncr::dynamic_bitset<uint8_t> bits(2);
		bits.from_vector({2, 0});
		std::string s = bits.to_string('.');
		cout << s << "\n";
		cout << bits << "\n";
	}

	// copy and move constructor test
	{
		cout << "\ncopy and move constructor test\n";
		ncr::dynamic_bitset<uint8_t> bits("01010100111");
		cout << bits << "\n";

		// calls the copy constructor
		ncr::dynamic_bitset<uint8_t> bits2 = bits;
		cout << bits2 << "\n";
		bits.reset(1);
		cout << bits << "\n";
		cout << bits2 << "\n";

		ncr::dynamic_bitset<uint8_t> bits3(std::move(bits));
		cout << bits3 << "\n";
		cout << "ptr is nullptr ? " << boolalpha << (bits._bits == nullptr) << "\n";
	}


	// resizing stuff
	{
		cout << "\nresizing\n";
		ncr::dynamic_bitset<uint8_t> bits;
		bits.resize(16);
		bits.set(1);
		bits.set(2);

		cout << bits << "\n";
		bits.resize(8);
		cout << bits << "\n";
		bits.resize(32);
		cout << bits << "\n";

		cout << "\nshrinking and extending\n";
		bits.reset();
		bits.resize(13);
		bits.set(0);
		bits.set(7);
		bits.set(12);
		cout << bits << "\n";
		bits.resize(16);
		cout << bits << "\n";
		bits.resize(32);
		cout << bits << "\n";
		bits.resize(8);
		cout << bits << "\n";
		bits.resize(16);
		cout << bits << "\n";

	}

	// assignment stuff
	{
		cout << "\nassignment\n";
		ncr::dynamic_bitset<uint8_t> bits1;
		ncr::dynamic_bitset<uint8_t> bits2;
		ncr::dynamic_bitset<uint8_t> bits3;

		// initialize
		bits1.resize(16);
		bits2.resize(32);
		bits3.resize(8);

		bits1.set(1);
		bits1.set(3);
		bits1.set(15);

		// before
		cout << "before assign\n";
		cout << bits1 << "\n";
		cout << bits2 << "\n";
		cout << bits3 << "\n";

		// after
		cout << "after assign\n";
		bits2.assign(bits1);
		bits3.assign(bits1);

		cout << bits1 << "\n";
		cout << bits2 << "\n";
		cout << bits3 << "\n";

		try {
			// this should throw a length error (bits1 is shorter than bits2)
			bits1 = bits2;
		}
		catch (std::length_error &e) {
			cout << "caught length error: " << e.what() << "\n";
		}
	}

}

int
main(int, char*[])
{
	test_bitset();
	std::cout << "\n";
	test_dynamic_bitset();
}
