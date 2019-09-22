#pragma once

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <array>
#include <sstream>

namespace nano_pow_server
{
/** A generic wrapper around multiprecision integers, adding convenience functions */
template <typename T, size_t SIZE_BYTES>
struct bigint
{
	bigint () = default;
	bigint (T val)
	{
		set (val);
	}

	bigint (std::string const & val)
	{
		from_hex (val);
	}

	void set (T const & number_a)
	{
		bytes.fill (0);
		boost::multiprecision::export_bits (number_a, bytes.rbegin (), 8, false);
	}

	T number () const
	{
		T result;
		boost::multiprecision::import_bits (result, bytes.begin (), bytes.end ());
		return result;
	}

	std::string to_dec () const
	{
		std::stringstream stream;
		stream << std::dec << std::noshowbase;
		stream << number ();
		return stream.str ();
	}

	std::string to_hex () const
	{
		std::stringstream stream;
		stream << std::hex << std::uppercase << std::noshowbase << std::setw (SIZE_BYTES * 2) << std::setfill ('0');
		stream << number ();
		return stream.str ();
	}

	void from_hex (std::string const & text)
	{
		if (!text.empty () && text.size () <= SIZE_BYTES * 2)
		{
			std::stringstream stream (text);
			stream << std::hex << std::noshowbase;

			T number;
			stream >> number;
			set (number);
			if (!stream.eof ())
			{
				throw std::runtime_error ("from_hex failed: invalid input format");
			}
		}
		else
		{
			throw std::runtime_error ("from_hex failed: invalid input size");
		}
	}

	union
	{
		std::array<uint8_t, SIZE_BYTES> bytes;
		std::array<uint64_t, SIZE_BYTES / sizeof (uint64_t)> qwords;
	};
};

using u128 = bigint<boost::multiprecision::uint128_t, 16>;
using u256 = bigint<boost::multiprecision::uint256_t, 32>;
using u512 = bigint<boost::multiprecision::uint512_t, 64>;
using bigfloat = boost::multiprecision::cpp_bin_float_100;

inline double to_multiplier (nano_pow_server::u128 const difficulty_a, nano_pow_server::u128 const base_difficulty_a)
{
	assert (difficulty_a.number () > 0);
	bigfloat res = (bigfloat (difficulty_a.number ()) / bigfloat (base_difficulty_a.number ()));
	return res.convert_to<double> ();
}

inline nano_pow_server::u128 from_multiplier (double const multiplier_a, nano_pow_server::u128 const base_difficulty_a)
{
	bigfloat res = bigfloat (base_difficulty_a.number ()) * bigfloat (multiplier_a);
	return res.convert_to<boost::multiprecision::uint128_t> ();
}
}
