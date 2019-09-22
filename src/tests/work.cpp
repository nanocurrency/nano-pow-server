#include <gtest/gtest.h>

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <vector>

#include <workserver/util.hpp>
#include <workserver/work_handler.hpp>

TEST (bigint, basic)
{
	nano_pow_server::u128 val128;
	ASSERT_NO_THROW (val128.from_hex (std::string (32, 'F')));
	ASSERT_EQ (val128.to_hex (), std::string (32, 'F'));
	ASSERT_EQ (val128.bytes.size (), 16);
	ASSERT_EQ (val128.number (), std::numeric_limits<boost::multiprecision::uint128_t>::max ());
	val128.set (100);
	ASSERT_EQ (val128.number (), 100);

	nano_pow_server::u256 val256;
	ASSERT_NO_THROW (val256.from_hex (std::string (64, 'F')));
	ASSERT_EQ (val256.to_hex (), std::string (64, 'F'));
	ASSERT_EQ (val256.bytes.size (), 32);
	ASSERT_EQ (val256.number (), std::numeric_limits<boost::multiprecision::uint256_t>::max ());

	nano_pow_server::u512 val512;
	ASSERT_NO_THROW (val512.from_hex (std::string (128, 'F')));
	ASSERT_EQ (val512.to_hex (), std::string (128, 'F'));
	ASSERT_EQ (val512.bytes.size (), 64);
	ASSERT_EQ (val512.number (), std::numeric_limits<boost::multiprecision::uint512_t>::max ());
}

TEST (queue, priority)
{
	nano_pow_server::config config;
	nano_pow_server::work_handler handler (config, nullptr);

	constexpr unsigned count = 10;
	constexpr unsigned count_priority_boosted = 4;
	std::vector<nano_pow_server::job> jobs (count);
	for (unsigned i = 0; i < count; i++)
	{
		jobs[i].request.root_hash = nano_pow_server::u256 (i);

		// Increase priority on the last N items, make sure they're sorted first,
		// yet stable-sorted on job id (i.e. jobs are FIFO within priority)
		jobs[i].set_priority (i >= (count - count_priority_boosted) ? 100 : i);
		handler.push_job (jobs[i]);
	}

	unsigned prev_id = 0;
	for (unsigned i = 0; i < count; i++)
	{
		auto job (*handler.pop_job ());
		if (i < count_priority_boosted)
		{
			ASSERT_EQ (100, job.get_priority ());
			if (i > 0)
			{
				ASSERT_GT (job.get_job_id (), prev_id);
			}
		}
		else
		{
			ASSERT_GT (100, job.get_priority ());
		}
		prev_id = job.get_job_id ();
	}
}

TEST (queue, remove)
{
	nano_pow_server::config config;
	nano_pow_server::work_handler handler (config, nullptr);

	constexpr unsigned count = 10;
	std::vector<nano_pow_server::job> jobs (count);
	for (unsigned i = 0; i < count; i++)
	{
		jobs[i].request.root_hash = nano_pow_server::u256 (i);
		handler.push_job (jobs[i]);
	}

	ASSERT_EQ (handler.get_queue ().size (), 10);
	handler.remove_job (nano_pow_server::u256 (5));
	ASSERT_EQ (handler.get_queue ().size (), 9);
}

TEST (difficulty, low)
{
	double expected_multiplier = 1.0;
	nano_pow_server::u128 base (0x2000000000000000);
	nano_pow_server::u128 diff (0x2000000000000001);
	double multiplier = nano_pow_server::to_multiplier (diff, base);
	ASSERT_NEAR (expected_multiplier, multiplier, 1e-10);
	std::cout << multiplier << std::endl;
}

TEST (difficulty, high)
{
	double expected_multiplier = 7.9999770129;
	nano_pow_server::u128 base (0x2000000000000000);
	nano_pow_server::u128 diff (0xffffcfcaef105e00);
	double multiplier = nano_pow_server::to_multiplier (diff, base);
	ASSERT_NEAR (expected_multiplier, multiplier, 1e-10);
}

TEST (difficulty, max)
{
	nano_pow_server::u128 base (0x2000000000000000);
	nano_pow_server::u128 diff;
	diff.from_hex ("100000000000000000000");
	double multiplier = nano_pow_server::to_multiplier (diff, base);
	auto diff_from_multiplier = nano_pow_server::from_multiplier (multiplier, base).number ();

	// Max multiplier is 2^(80-61)
	ASSERT_EQ (multiplier, 0x80000);
	ASSERT_EQ (diff_from_multiplier, diff.number ());
}
