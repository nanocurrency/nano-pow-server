#pragma once

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/optional.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>
#include <workserver/config.hpp>
#include <workserver/util.hpp>

// Mock types for the pow backend
namespace nano_pow
{
	class driver
	{
	};
	class cpp_driver : public driver
	{
	};
	class opencl_driver : public driver
	{
	};
}

namespace nano_pow_server
{
/**
 * A job is a queued work generation request with an optional priority. Jobs are stable-sorted,
 * which means it's a FIFO queue within each priority level.
 */
class job
{
public:
	/** Constructor sets a unique job id */
	job ();
	std::chrono::time_point<std::chrono::system_clock> start_time;
	std::chrono::time_point<std::chrono::system_clock> end_time;

	struct request
	{
		u256 root_hash{ "0" };
		u128 difficulty{ "0" };
		double multiplier{ 1.0 };
	} request;

	struct result
	{
		u128 work{ "0" };
		u128 difficulty{ "0" };
		double multiplier{ 1.0 };
	} result;

	void start ()
	{
		start_time = std::chrono::system_clock::now ();
	}

	void stop ()
	{
		end_time = std::chrono::system_clock::now ();
	}

	std::chrono::milliseconds duration ()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds> (end_time - start_time);
	}

	unsigned get_job_id () const
	{
		return job_id;
	}

	unsigned get_priority () const
	{
		return priority;
	}

	void set_priority (unsigned priority_a)
	{
		priority = priority_a;
	}

	/** Stable sorted priority queue */
	struct comparator
	{
		bool operator() (job const & job1, job const & job2) const
		{
			if (job1.priority != job2.priority)
			{
				return job2.priority > job1.priority;
			}
			return job1.job_id > job2.job_id;
		}
	};

private:
	unsigned priority{ 0 };
	unsigned job_id{ 0 };
	static std::atomic<unsigned> job_id_dispenser;
};

/** Parses and processes work requests */
class work_handler
{
public:
	/** We register a nano_pow device for every device entry in the config file */
	class registered_device
	{
	public:
		registered_device (nano_pow_server::config::device device_config_a,
		    std::shared_ptr<nano_pow::driver> const & driver_a)
		    : device_config (device_config_a)
		    , driver (driver_a)
		{
		}
		registered_device (registered_device const & other)
		{
			device_config = other.device_config;
			driver = other.driver;
			busy = other.busy.load ();
		}

		/** Sets the busy flag and returns the previous busy state. If the returned value is true, the device is already busy. */
		bool try_aquire ()
		{
			return busy.exchange (true);
		}

		/** Clears the busy flag. If the returned flag is false, the device was already released (i.e in a non-busy state) */
		bool release ()
		{
			return busy.exchange (false);
		}

		nano_pow_server::config::device device_config;
		std::shared_ptr<nano_pow::driver> driver;
		std::atomic<bool> busy{ false };
	};

	work_handler (nano_pow_server::config const & config_a, std::shared_ptr<spdlog::logger> const & logger_a);
	~work_handler ();

	/**
	 * Parse JSON work generation request.
	 * Returns immediately and delivers the result by calling \p response_handler
	 */
	void handle_request_async (std::string body, std::function<void(std::string)> response_handler);

	/**
	 * Emits queue information in json format
	 */
	void handle_queue_request (std::function<void(std::string)> response_handler);

	/**
	 * Clears the work queue. Requires config option server.allow_control to be true.
	 */
	void handle_queue_delete_request (std::function<void(std::string)> response_handler);

	/** Pushes a copy of \p job into the job queue */
	void push_job (nano_pow_server::job const & job)
	{
		std::lock_guard<std::mutex> lk (jobs_mutex);
		jobs.emplace (job);
	}

	/**
	 * Removes a pending work request from the queue
	 * @return true if the \p root_hash was found and removed
	 */
	bool remove_job (u256 root_hash)
	{
		std::lock_guard<std::mutex> lk (jobs_mutex);

		// It's not possible to remove from priority queues. As cancelling work is an infrequent request, we
		// simply reconstruct the queue through a filter.
		std::priority_queue<job, std::vector<job>, job::comparator> jobs_l;
		bool removed = false;
		while (!jobs.empty ())
		{
			auto current (jobs.top ());
			if (current.request.root_hash.number () != root_hash.number ())
			{
				jobs_l.push (current);
			}
			else
			{
				removed = true;
			}

			jobs.pop ();
		}
		jobs.swap (jobs_l);
		return removed;
	}

	/** Returns the next highest priority job, or boost::none if no jobs are available */
	boost::optional<nano_pow_server::job> pop_job ()
	{
		std::lock_guard<std::mutex> lk (jobs_mutex);
		boost::optional<nano_pow_server::job> res;
		if (!jobs.empty ())
		{
			res = jobs.top ();
			jobs.pop ();
		}
		return res;
	}

	std::priority_queue<job, std::vector<job>, job::comparator> const & get_queue () const
	{
		return jobs;
	}

	registered_device & aquire_first_available_device ()
	{
		auto it = std::find_if (devices.begin (), devices.end (), [](registered_device & driver) { return !driver.try_aquire (); });
		if (it != devices.end ())
		{
			return *it;
		}
		throw std::runtime_error ("No available devices");
	}

private:
	std::vector<registered_device> devices;
	nano_pow_server::config const & config;
	std::shared_ptr<spdlog::logger> logger;
	boost::asio::thread_pool pool;

	std::mutex jobs_mutex;
	std::priority_queue<job, std::vector<job>, job::comparator> jobs;

	std::mutex active_jobs_mutex;
	std::set<std::reference_wrapper<job>, job::comparator> active_jobs;

	std::mutex completed_jobs_mutex;
	boost::circular_buffer<job> completed_jobs{ 128 };
};
}
