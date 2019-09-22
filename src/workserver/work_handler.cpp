#include <boost/multiprecision/cpp_int.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <array>
#include <thread>

#include <nano_pow/cpp_driver.hpp>
#include <nano_pow/opencl_driver.hpp>
#include <workserver/work_handler.hpp>

using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
std::atomic<unsigned> nano_pow_server::job::job_id_dispenser{ 1 };

nano_pow_server::job::job ()
{
	job_id = job_id_dispenser.fetch_add (1);
}

/** Currently we support work on a single device, hence a single-thread pool */
nano_pow_server::work_handler::work_handler (nano_pow_server::config const & config_a, std::shared_ptr<spdlog::logger> const & logger_a)
    : config (config_a)
    , logger (logger_a)
    , pool (config.devices.size ())
{
	for (auto device : config.devices)
	{
		uint64_t memory{ device.memory };
		std::shared_ptr<nano_pow::driver> driver;
		if (device.type == nano_pow_server::config::device::device_type::cpu)
		{
			driver = std::make_shared<nano_pow::cpp_driver> ();
		}
		else if (device.type == nano_pow_server::config::device::device_type::gpu)
		{
			driver = std::make_shared<nano_pow::opencl_driver> ();
		}

		// TOOD:
		driver->memory_set (memory);

		devices.emplace_back (device, driver);
	}
}

nano_pow_server::work_handler::~work_handler ()
{
	pool.stop ();
}

void nano_pow_server::work_handler::handle_queue_request (std::function<void(std::string)> response_handler)
{
	std::unique_lock<std::mutex> lk (jobs_mutex);
	std::unique_lock<std::mutex> lk_active (active_jobs_mutex);
	std::unique_lock<std::mutex> lk_completed (completed_jobs_mutex);

	boost::property_tree::ptree response;

	auto jobs_l = jobs;
	lk.unlock ();

	auto populate_json = [](boost::property_tree::ptree & json_job, job const & job_a) {
		json_job.put ("id", job_a.get_job_id ());
		json_job.put ("priority", job_a.get_priority ());
		json_job.put ("start", std::chrono::duration_cast<std::chrono::milliseconds> (job_a.start_time.time_since_epoch ()).count ());
		json_job.put ("end", std::chrono::duration_cast<std::chrono::milliseconds> (job_a.end_time.time_since_epoch ()).count ());

		boost::property_tree::ptree request;
		request.put ("hash", job_a.request.root_hash.to_hex ());
		request.put ("difficulty", job_a.request.difficulty.to_hex ());
		request.put ("multiplier", job_a.request.multiplier);
		json_job.add_child ("request", request);

		boost::property_tree::ptree result;
		result.put ("work", job_a.result.work.to_hex ());
		result.put ("difficulty", job_a.result.difficulty.to_hex ());
		result.put ("multiplier", job_a.result.multiplier);
		json_job.add_child ("result", result);
	};

	boost::property_tree::ptree child_queued_jobs;
	while (!jobs_l.empty ())
	{
		auto current (jobs_l.top ());
		boost::property_tree::ptree json_job;
		populate_json (json_job, current);
		child_queued_jobs.push_back (std::make_pair ("", json_job));
		jobs_l.pop ();
	}
	response.add_child ("queued", child_queued_jobs);

	boost::property_tree::ptree child_active_jobs;
	for (auto current : active_jobs)
	{
		boost::property_tree::ptree json_job;
		populate_json (json_job, current.get ());
		child_active_jobs.push_back (std::make_pair ("", json_job));
	}
	response.add_child ("active", child_active_jobs);

	boost::property_tree::ptree child_completed_jobs;
	for (auto const & current : completed_jobs)
	{
		boost::property_tree::ptree json_job;
		populate_json (json_job, current);
		child_completed_jobs.push_back (std::make_pair ("", json_job));
	}
	response.add_child ("completed", child_completed_jobs);

	std::stringstream ostream;
	boost::property_tree::write_json (ostream, response);
	response_handler (ostream.str ());
}

void nano_pow_server::work_handler::handle_queue_delete_request (std::function<void(std::string)> response_handler)
{
	boost::property_tree::ptree response;

	if (config.server.allow_control)
	{
		std::unique_lock<std::mutex> lk (jobs_mutex);
		jobs = decltype (jobs) ();
		logger->warn ("Queue removed via RPC");
		response.put ("success", true);
	}
	else
	{
		response.put ("error", "Control requests are not allowed. This must be enabled in the server configuration.");
	}

	std::stringstream ostream;
	boost::property_tree::write_json (ostream, response);
	response_handler (ostream.str ());
}

void nano_pow_server::work_handler::handle_request_async (std::string body, std::function<void(std::string)> response_handler)
{
	auto attach_correlation_id = [](boost::optional<std::string> const & id, boost::property_tree::ptree & response) {
		if (id)
		{
			response.put ("id", id.get ());
		}
	};

	auto create_error_response = [&attach_correlation_id](boost::optional<std::string> const & id, std::string const & error_a) {
		std::stringstream ostream;
		boost::property_tree::ptree response;
		response.put ("error", error_a);
		attach_correlation_id (id, response);
		boost::property_tree::write_json (ostream, response);
		return ostream.str ();
	};

	// Optional correlation id (necessary to match responses with requests when using WebSockets,
	// though POST requests can include them too)
	boost::optional<std::string> correlation_id;

	try
	{
		std::stringstream istream (body);
		boost::property_tree::ptree request;
		boost::property_tree::read_json (istream, request);

		correlation_id = request.get_optional<std::string> ("id");

		auto action (request.get_optional<std::string> ("action"));
		if (action && *action == "work_generate")
		{
			if (config.devices.empty ())
			{
				throw std::runtime_error ("No work device has been configured");
			}

			job job_l;

			auto root_hash (request.get_optional<std::string> ("hash"));
			if (!root_hash.is_initialized ())
			{
				throw std::runtime_error ("work_generate failed: missing hash value");
			}
			u256 hash (*root_hash);
			job_l.request.root_hash = hash;

			job_l.request.difficulty = config.work.base_difficulty;
			auto difficulty_hex (request.get_optional<std::string> ("difficulty"));
			if (difficulty_hex.is_initialized ())
			{
				job_l.request.difficulty.from_hex (*difficulty_hex);
			}

			double multiplier = request.get<double> ("multiplier", .0);
			if (multiplier > .0)
			{
				job_l.request.difficulty = from_multiplier (multiplier, config.work.base_difficulty);
			}

			if (config.server.allow_prioritization)
			{
				job_l.set_priority (request.get<unsigned> ("priority", 0));
			}

			logger->info ("Work requested. Root hash: {}, difficulty: {}, priority: {}",
			    job_l.request.root_hash.to_hex (), job_l.request.difficulty.to_hex (), job_l.get_priority ());

			// Queue the request as a job
			{
				std::unique_lock<std::mutex> lk (jobs_mutex);
				if (jobs.size () < config.server.request_limit)
				{
					jobs.push (job_l);
				}
				else
				{
					throw std::runtime_error ("Work request limit exceeded");
				}
			}

			// The thread pool size is the same as the driver count. As a result, we know that a driver
			// will be available whenever the pool handler is called.
			boost::asio::post (pool, [this, correlation_id, response_handler, create_error_response, attach_correlation_id] {
				std::unique_lock<std::mutex> lk (jobs_mutex);
				if (!jobs.empty ())
				{
					auto job = jobs.top ();
					jobs.pop ();
					lk.unlock ();

					try
					{
						auto & device = aquire_first_available_device ();
						logger->info ("Thread {0:x} generating work on {1} for root {2}",
						    std::hash<std::thread::id>{}(std::this_thread::get_id ()),
						    device.device_config.type_as_string (),
						    job.request.root_hash.to_hex ());

						job.start ();

						{
							std::unique_lock<std::mutex> lk (active_jobs_mutex);
							active_jobs.insert (job);
						}

						// TODO: set difficulty when handling request once the nano-pow API is set
						//       as well as set_memory based on request?
						// driver->difficulty_set(nano_pow::reverse ((1ULL << 40) - 1));

						// TODO: nano-pow API currently takes a 64 bit integer, same with the nonce

						// The lower 128 bits of the 256 bit root is used as nonce
						//std::array<uint64_t, 2> nonce {job.request.root_hash.qwords[1], job.request.root_hash.qwords[0]};
						//device.driver->difficulty_set(job_l.request.difficulty);
						//uint64_t solution = device.driver->solve(nonce);

						boost::property_tree::ptree response;

						if (config.work.mock_work_generation_delay == 0)
						{
							// TODO: use values from nano-pow / to_multiplier ()
							job.result.work = u128 ("2feaeaa000000000");
							job.result.difficulty = u128 ("2000000000000000");
							job.result.multiplier = 1.0;
						}
						else
						{
							// Mock response for testing
							std::this_thread::sleep_for (std::chrono::seconds (config.work.mock_work_generation_delay));
							job.result.work = u128 ("2feaeaa000000000");
							job.result.difficulty = u128 ("0x2ffee0000000000");
							job.result.multiplier = 1.3847;
							response.put ("testing", true);
						}

						response.put ("work", job.result.work.to_hex ());
						response.put ("difficulty", job.result.difficulty.to_hex ());
						response.put ("multiplier", job.result.multiplier);
						attach_correlation_id (correlation_id, response);

						std::stringstream ostream;
						boost::property_tree::write_json (ostream, response);
						response_handler (ostream.str ());

						device.release ();
						job.stop ();

						std::unique_lock<std::mutex> lk_active (active_jobs_mutex);
						std::unique_lock<std::mutex> lk_completed (completed_jobs_mutex);
						active_jobs.erase (job);
						completed_jobs.push_back (job);

						logger->info ("Work completed in {} ms for hash {} ", job.duration ().count (), job.request.root_hash.to_hex ());
					}
					catch (std::runtime_error const & ex)
					{
						response_handler (create_error_response (correlation_id, ex.what ()));
					}
				}
				else
				{
					response_handler (create_error_response (correlation_id, "No jobs available"));
				}
			});
		}
		else if (action && *action == "work_validate")
		{
			auto hash_hex (request.get_optional<std::string> ("hash"));
			if (!hash_hex.is_initialized ())
			{
				throw std::runtime_error ("work_generate failed: missing hash value");
			}
			u256 hash (*hash_hex);

			auto work_hex (request.get_optional<std::string> ("work"));
			if (!work_hex.is_initialized ())
			{
				throw std::runtime_error ("work_generate failed: missing work value");
			}
			u128 work (*work_hex);

			u128 difficulty = config.work.base_difficulty;
			auto difficulty_hex (request.get_optional<std::string> ("difficulty"));
			if (difficulty_hex.is_initialized ())
			{
				difficulty.from_hex (*difficulty_hex);
			}

			double multiplier = request.get<double> ("multiplier", .0);
			if (multiplier > .0)
			{
				difficulty = from_multiplier (multiplier, config.work.base_difficulty);
			}

			std::array<uint64_t, 2> nonce{ hash.qwords[1], hash.qwords[0] };

			// TODO: update to use full 128 bit work/difficulty when the nano_pow API is updated
			bool passes (nano_pow::passes (nonce, work.qwords[0], difficulty.qwords[0]));

			boost::property_tree::ptree response;
			response.put ("valid", passes ? "1" : "0");

			// TODO: add these when the nano_pow API is done
			response.put ("difficulty", "0x2000000000000000");
			response.put ("multiplier", "1.0"); // TODO: to_multiplier ()
			attach_correlation_id (correlation_id, response);

			std::stringstream ostream;
			boost::property_tree::write_json (ostream, response);
			response_handler (ostream.str ());
		}
		else if (action && *action == "work_cancel")
		{
			auto hash_hex (request.get_optional<std::string> ("hash"));
			if (!hash_hex.is_initialized ())
			{
				throw std::runtime_error ("work_cancel failed: missing hash value");
			}
			u256 hash;
			hash.from_hex (*hash_hex);

			if (remove_job (hash))
			{
				logger->info ("Cancelled work request for root {}", hash.to_hex ());

				// The old work server always returned an empty response, while now we
				// write a status. This should not break any existing clients.
				boost::property_tree::ptree response;
				response.put ("status", "cancelled");
				attach_correlation_id (correlation_id, response);

				std::stringstream ostream;
				boost::property_tree::write_json (ostream, response);
				response_handler (ostream.str ());
			}
			else
			{
				throw std::runtime_error ("Hash not found in work queue");
			}
		}
		else
		{
			throw std::runtime_error ("Invalid action field");
		}
	}
	catch (std::runtime_error const & ex)
	{
		logger->info ("An error occurred and will be reported to the client: {}", ex.what ());
		response_handler (create_error_response (correlation_id, ex.what ()));
	}
}
