#pragma once

#include <boost/filesystem.hpp>

#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include <cpptoml.h>
#include <workserver/util.hpp>

namespace nano_pow_server
{
static const char * BASE_DIFFICULTY = "2000000000000000";

/** POW server configuration, loaded from the TOML file and command line overrides (both of which are optional) */
class config
{
public:
	/** Work server related settings*/
	class server
	{
	public:
		/** Web server/websocket listening address */
		std::string bind_address{ "0.0.0.0" };
		/** Web server/websocket listening port */
		uint16_t port{ 8076 };
		/** Number of web server IO threads. Since work generation is done asynchronously, a low thread count should suffice */
		uint16_t threads{ static_cast<uint16_t> (std::thread::hardware_concurrency ()) };
		/** The maximum size of the request queue. Attempt to request work when full will result in an error reply */
		uint32_t request_limit{ 1024 * 16 };
		/** If true, the work server will honor the priority field in requests */
		bool allow_prioritization{ true };
		/** If true, allow control requests, such as clearing the queue */
		bool allow_control{ false };
		/** If true, log to stderr in addition to file */
		bool log_to_stderr{ false };
	} server;

	/** Device settings */
	class device
	{
	public:
		enum class device_type
		{
			cpu,
			gpu
		};
		device_type type{ device_type::cpu };
		unsigned platform{ 0 };
		unsigned device{ 0 };
		unsigned threads{ 0 };
		/** How much memory to request when calling nano-pow, in bytes */
		uint64_t memory{ 1024 * 1024 * 1024 * 2ULL };

		std::string type_as_string ()
		{
			switch (type)
			{
				case device_type::cpu:
					return "cpu";
				case device_type::gpu:
					return "gpu";
				default:
					return "unknown";
			}
		}
	};
	std::vector<device> devices;

	/** Work related settings */
	class work
	{
	public:
		work ()
		{
			base_difficulty.from_hex (BASE_DIFFICULTY);
		}
		u128 base_difficulty;
		/** If set, the work_generate RPC will not attempt real work generation but simply return mock data. This is useful testing. */
		uint16_t mock_work_generation_delay{ 0 };
	} work;

	/** Admin UI settings */
	class admin
	{
	public:
		std::string doc_root{ "public" };
		bool allow_remote{ false };
	} admin;

	/** Read the TOML file \p toml_config_path if it exists, and override any settings using \p config_overrides */
	config (std::string const & toml_config_path = "", std::vector<std::string> const & config_overrides = std::vector<std::string> ())
	{
		read_config (toml_config_path, config_overrides);
		if (tree->contains ("server"))
		{
			auto server_l (tree->get_table ("server"));
			server.bind_address = server_l->get_as<std::string> ("bind").value_or (server.bind_address);
			server.port = server_l->get_as<uint16_t> ("port").value_or (server.port);
			server.threads = server_l->get_as<uint16_t> ("threads").value_or (server.threads);
			server.allow_prioritization = server_l->get_as<bool> ("allow_prioritization").value_or (server.allow_prioritization);
			server.allow_control = server_l->get_as<bool> ("allow_control").value_or (server.allow_control);
			server.request_limit = server_l->get_as<uint32_t> ("request_limit").value_or (server.request_limit);
			server.log_to_stderr = server_l->get_as<bool> ("log_to_stderr").value_or (server.log_to_stderr);
		}

		if (tree->contains ("work"))
		{
			auto work_l (tree->get_table ("work"));
			auto base_hex_l = work_l->get_as<std::string> ("base_difficulty").value_or (BASE_DIFFICULTY);
			work.base_difficulty.from_hex (base_hex_l);
			work.mock_work_generation_delay = work_l->get_as<uint16_t> ("mock_work_generation_delay").value_or (work.mock_work_generation_delay);
		}

		if (tree->contains ("device"))
		{
			auto get_device = [&](std::shared_ptr<cpptoml::table> dev_a) {
				device dev;
				dev.platform = dev_a->get_as<unsigned> ("platform").value_or (dev.platform);
				dev.device = dev_a->get_as<unsigned> ("device").value_or (dev.device);
				dev.threads = dev_a->get_as<unsigned> ("threads").value_or (dev.threads);
				dev.memory = dev_a->get_as<unsigned> ("memory").value_or (dev.memory);
				std::string type_l (dev_a->get_as<std::string> ("type").value_or ("cpu"));
				if (type_l == "cpu")
				{
					dev.type = device::device_type::cpu;
				}
				else if (type_l == "gpu")
				{
					dev.type = device::device_type::gpu;
				}
				else
				{
					throw std::runtime_error ("config: invalid device type (must be \"cpu\" or \"gpu\")");
				}
				return dev;
			};

			// We support both a single [device] entry, as well as a TOML table array of [[device]] entries
			auto device_l = tree->get ("device");
			if (device_l->is_table ())
			{
				auto dev = get_device (device_l->as_table ());
				devices.emplace_back (dev);
			}
			else if (device_l->is_table_array ())
			{
				for (auto & table : *device_l->as_table_array ())
				{
					auto dev = get_device (table);
					devices.emplace_back (dev);
				}
			}
			else
			{
				throw std::runtime_error ("device must be a table or table array");
			}
		}
		if (tree->contains ("admin"))
		{
			auto admin_l (tree->get_table ("admin"));
			admin.doc_root = admin_l->get_as<std::string> ("path").value_or (admin.doc_root);
			admin.allow_remote = admin_l->get_as<bool> ("allow_remote").value_or (admin.allow_remote);
		}
	}

private:
	std::shared_ptr<cpptoml::table> tree;

	void read_config (std::string const & toml_config_path, std::vector<std::string> const & config_overrides = std::vector<std::string> ())
	{
		std::stringstream config_overrides_stream;
		for (auto const & entry : config_overrides)
		{
			config_overrides_stream << entry << std::endl;
		}
		config_overrides_stream << std::endl;

		if (!toml_config_path.empty () && boost::filesystem::exists (toml_config_path))
		{
			std::ifstream input (toml_config_path);
			tree = cpptoml::parse_base_and_override_files (config_overrides_stream, input, cpptoml::parser::merge_type::ignore, false);
		}
		else
		{
			std::stringstream stream_empty;
			stream_empty << std::endl;
			tree = cpptoml::parse_base_and_override_files (config_overrides_stream, stream_empty, cpptoml::parser::merge_type::ignore, false);
		}
	}
};
}
