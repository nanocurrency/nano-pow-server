#pragma once

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/config.hpp>
#include <boost/make_unique.hpp>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <vector>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;

/* Boost v1.70 introduced breaking changes; the conditional compilation allows 1.6x to be supported as well. */
#if BOOST_VERSION < 107000
using socket_type = boost::asio::ip::tcp::socket;
#define beast_buffers boost::beast::buffers
#else
using socket_type = boost::asio::basic_stream_socket<boost::asio::ip::tcp, boost::asio::io_context::executor_type>;
#define beast_buffers boost::beast::make_printable
#endif

namespace web
{
/** Configuration options passed to sessions */
class config
{
public:
	bool static_pages_allow_remote{ false };
	bool static_pages_allow{ true };
};

class http_session;
class websocket_session;

/** An HTTP REST endpoint registered with the webserver */
class rest_endpoint
{
public:
	rest_endpoint (http::verb verb_a,
	    std::string path_verbatim_a,
	    std::regex path_a,
	    std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler_a)
	    : verb (verb_a)
	    , path_verbatim (path_verbatim_a)
	    , path (path_a)
	    , handler (handler_a)
	{
	}

	http::verb verb;
	std::string path_verbatim;
	std::regex path;
	std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler;
};

/** A WebSocket endpoint registered with the webserver */
class websocket_endpoint
{
public:
	websocket_endpoint (std::string path_verbatim_a,
	    std::regex path_a,
	    std::function<void(std::string, std::shared_ptr<websocket_session>)> handler_a)
	    : path_verbatim (path_verbatim_a)
	    , path (path_a)
	    , handler (handler_a)
	{
	}

	std::string path_verbatim;
	std::regex path;
	std::function<void(std::string, std::shared_ptr<websocket_session>)> handler;
};

/** A bare minimum uri parser to get path and query */
class url
{
public:
	url (std::string const & url_a)
	{
		std::string::const_iterator path_i = std::find (url_a.begin (), url_a.end (), '/');
		std::string::const_iterator query_i = std::find (path_i, url_a.end (), '?');
		path.assign (path_i, query_i);
		if (query_i != url_a.end ())
		{
			++query_i;
		}
		query.assign (query_i, url_a.end ());
	}

	std::string path, query;

private:
	void parse (const std::string & url_s);
};

boost::beast::string_view mime_type (boost::beast::string_view path)
{
	using boost::beast::iequals;
	auto const ext = [&path] {
		auto const pos = path.rfind (".");
		if (pos == boost::beast::string_view::npos)
		{
			return boost::beast::string_view{};
		}
		return path.substr (pos);
	}();
	if (iequals (ext, ".htm"))
		return "text/html";
	if (iequals (ext, ".html"))
		return "text/html";
	if (iequals (ext, ".css"))
		return "text/css";
	if (iequals (ext, ".txt"))
		return "text/plain";
	if (iequals (ext, ".js"))
		return "application/javascript";
	if (iequals (ext, ".json"))
		return "application/json";
	if (iequals (ext, ".xml"))
		return "application/xml";
	if (iequals (ext, ".png"))
		return "image/png";
	if (iequals (ext, ".jpe"))
		return "image/jpeg";
	if (iequals (ext, ".jpeg"))
		return "image/jpeg";
	if (iequals (ext, ".jpg"))
		return "image/jpeg";
	if (iequals (ext, ".gif"))
		return "image/gif";
	if (iequals (ext, ".bmp"))
		return "image/bmp";
	if (iequals (ext, ".ico"))
		return "image/vnd.microsoft.icon";
	if (iequals (ext, ".tiff"))
		return "image/tiff";
	if (iequals (ext, ".tif"))
		return "image/tiff";
	if (iequals (ext, ".svg"))
		return "image/svg+xml";
	if (iequals (ext, ".svgz"))
		return "image/svg+xml";
	return "application/text";
}

/**
 * Append an HTTP rel-path to a local filesystem path.
 * @return Path is normalized for the platform.
 */
std::string path_cat (boost::beast::string_view base, boost::beast::string_view path)
{
	if (base.empty ())
	{
		return path.to_string ();
	}
	std::string result = base.to_string ();
#if BOOST_MSVC
	char constexpr path_separator = '\\';
	if (result.back () == path_separator)
	{
		result.resize (result.size () - 1);
	}
	result.append (path.data (), path.size ());
	for (auto & c : result)
	{
		if (c == '/')
		{
			c = path_separator;
		}
	}
#else
	char constexpr path_separator = '/';
	if (result.back () == path_separator)
	{
		result.resize (result.size () - 1);
	}
	result.append (path.data (), path.size ());
#endif
	return result;
}

void fail (boost::system::error_code ec, char const * what)
{
	std::cerr << what << ": " << ec.message () << "\n";
}

/** WebSocket session created by the web server upon a websocket upgrade */
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
	websocket::stream<socket_type> ws_;
	boost::asio::strand<
	    boost::asio::io_context::executor_type>
	    strand_;
	boost::asio::steady_timer timer_;
	boost::beast::multi_buffer buffer_;
	char ping_state_ = 0;
	/** Outgoing messages. The send queue is protected by accessing it only through the strand */
	std::deque<std::string> send_queue_;
	websocket_endpoint const & handler_;

public:
	explicit websocket_session (socket_type socket, websocket_endpoint const & handler)
	    : ws_ (std::move (socket))
	    , strand_ (ws_.get_executor ())
	    , timer_ (ws_.get_executor ().context (), (std::chrono::steady_clock::time_point::max) ())
	    , handler_ (handler)
	{
	}

	void write_response (std::string message_a)
	{
		auto this_l (shared_from_this ());
		boost::asio::post (strand_,
		    [message_a, this_l]() {
			    bool write_in_progress = !this_l->send_queue_.empty ();
			    this_l->send_queue_.emplace_back (message_a);
			    if (!write_in_progress)
			    {
				    this_l->write_queued_messages ();
			    }
		    });
	}

	void write_queued_messages ()
	{
		auto msg (send_queue_.front ());
		auto this_l (shared_from_this ());

		ws_.async_write (boost::asio::buffer (msg),
		    boost::asio::bind_executor (strand_,
		        [this_l](boost::system::error_code ec, std::size_t bytes_transferred) {
			        this_l->send_queue_.pop_front ();
			        if (!ec)
			        {
				        if (!this_l->send_queue_.empty ())
				        {
					        this_l->write_queued_messages ();
				        }
			        }
		        }));
	}

	template <class Body, class Allocator>
	void do_accept (http::request<Body, http::basic_fields<Allocator>> req)
	{
		// Control callback to handle ping, pong, and close frames.
		ws_.control_callback (std::bind (&websocket_session::on_control_callback, this, std::placeholders::_1, std::placeholders::_2));

		on_timer ({});
		timer_.expires_after (std::chrono::seconds (15));

		// Accept the websocket handshake
		ws_.async_accept (req, boost::asio::bind_executor (strand_, std::bind (&websocket_session::on_accept, shared_from_this (), std::placeholders::_1)));
	}

	void on_accept (boost::system::error_code ec)
	{
		if (ec != boost::asio::error::operation_aborted)
		{
			if (!ec)
			{
				read ();
			}
			else
			{
				fail (ec, "accept");
			}
		}
	}

	void on_timer (boost::system::error_code ec)
	{
		if (ec && ec != boost::asio::error::operation_aborted)
		{
			fail (ec, "timer");
		}
		else
		{
			// See if the timer really expired since the deadline may have moved.
			if (timer_.expiry () <= std::chrono::steady_clock::now ())
			{
				// If this is the first time the timer expired,
				// send a ping to see if the other end is there.
				if (ws_.is_open () && ping_state_ == 0)
				{
					// Note that we are sending a ping
					ping_state_ = 1;

					// Set the timer
					timer_.expires_after (std::chrono::seconds (15));

					// Now send the ping
					ws_.async_ping ({}, boost::asio::bind_executor (strand_, std::bind (&websocket_session::on_ping, shared_from_this (), std::placeholders::_1)));
				}
				else
				{
					// The timer expired while trying to handshake, or we sent a ping and it never completed or
					// we never got back a control frame, so close.

					// Closing the socket cancels all outstanding operations. They
					// will complete with boost::asio::error::operation_aborted
					ws_.next_layer ().shutdown (tcp::socket::shutdown_both, ec);
					ws_.next_layer ().close (ec);
					return;
				}
			}

			// Wait on the timer
			timer_.async_wait (boost::asio::bind_executor (strand_,
			    std::bind (&websocket_session::on_timer,
			        shared_from_this (),
			        std::placeholders::_1)));
		}
	}

	// Called to indicate activity from the remote peer
	void activity ()
	{
		// Note that the connection is alive
		ping_state_ = 0;

		// Set the timer
		timer_.expires_after (std::chrono::seconds (15));
	}

	// Called after a ping is sent.
	void on_ping (boost::system::error_code ec)
	{
		// Happens when the timer closes the socket
		if (ec != boost::asio::error::operation_aborted)
		{
			if (ec)
			{
				fail (ec, "ping");
			}
			else
			{
				// Note that the ping was sent.
				if (ping_state_ == 1)
				{
					ping_state_ = 2;
				}
			}
		}
	}

	void on_control_callback (websocket::frame_type kind, boost::beast::string_view payload)
	{
		boost::ignore_unused (kind, payload);
		activity ();
	}

	void read ()
	{
		ws_.async_read (buffer_,
		    boost::asio::bind_executor (
		        strand_,
		        std::bind (
		            &websocket_session::on_read,
		            shared_from_this (),
		            std::placeholders::_1,
		            std::placeholders::_2)));
	}

	void on_read (boost::system::error_code ec, std::size_t bytes_transferred)
	{
		boost::ignore_unused (bytes_transferred);

		// Happens when the timer closes the socket
		if (ec != boost::asio::error::operation_aborted && ec != websocket::error::closed)
		{
			if (!ec)
			{
				activity ();

				std::stringstream os;
				os << beast_buffers (buffer_.data ());
				std::string incoming_message = os.str ();

				handler_.handler (incoming_message, shared_from_this ());

				// Clear the buffer and issue another read
				buffer_.consume (buffer_.size ());
				read ();
			}
			else
			{
				fail (ec, "read");
			}
		}
	}
};

class listener;

/** Each HTTP connection is maintained by a http_session which manages it's own lifetime */
class http_session : public std::enable_shared_from_this<http_session>
{
	friend class listener;

	// HTTP pipeline
	class queue
	{
		enum
		{
			// Maximum number of responses we will queue
			limit = 8
		};

		// The type-erased, saved work item
		struct work
		{
			virtual ~work () = default;
			virtual void operator() () = 0;
		};

		http_session & self_;
		std::vector<std::unique_ptr<work>> items_;

	public:
		explicit queue (http_session & self)
		    : self_ (self)
		{
			static_assert (limit > 0, "queue limit must be positive");
			items_.reserve (limit);
		}

		// Returns `true` if we have reached the queue limit
		bool is_full () const
		{
			return items_.size () >= limit;
		}

		// Called when a message finishes sending
		// Returns `true` if the caller should initiate a read
		bool on_write ()
		{
			BOOST_ASSERT (!items_.empty ());
			auto const was_full = is_full ();
			items_.erase (items_.begin ());
			if (!items_.empty ())
			{
				(*items_.front ()) ();
			}
			return was_full;
		}

		// Called by the HTTP handler to send a response.
		template <bool isRequest, class Body, class Fields>
		void operator() (http::message<isRequest, Body, Fields> && msg)
		{
			// This holds a work item
			struct work_impl : work
			{
				http_session & self_;
				http::message<isRequest, Body, Fields> msg_;

				work_impl (
				    http_session & self,
				    http::message<isRequest, Body, Fields> && msg)
				    : self_ (self)
				    , msg_ (std::move (msg))
				{
				}

				void operator() ()
				{
					http::async_write (
					    self_.socket_,
					    msg_,
					    boost::asio::bind_executor (
					        self_.strand_,
					        std::bind (
					            &http_session::on_write,
					            self_.shared_from_this (),
					            std::placeholders::_1,
					            msg_.need_eof ())));
				}
			};

			// Allocate and store the work
			items_.push_back (boost::make_unique<work_impl> (self_, std::move (msg)));

			// If there was no previous work, start this one
			if (items_.size () == 1)
			{
				(*items_.front ()) ();
			}
		}
	};

	std::vector<rest_endpoint> const & handlers_;
	std::vector<websocket_endpoint> const & websocket_handlers_;
	socket_type socket_;
	tcp::endpoint remote_address_;
	boost::asio::strand<
	    boost::asio::io_context::executor_type>
	    strand_;
	boost::asio::steady_timer timer_;
	boost::beast::flat_buffer buffer_;
	std::string const & doc_root_;
	config const & config_;
	http::request<http::string_body> req_;
	queue queue_;
	// May be changed during requests
	unsigned client_version{ 11 };
	bool client_keepalive{ true };

public:
	explicit http_session (std::vector<rest_endpoint> const & handlers,
	    std::vector<websocket_endpoint> const & websocket_handlers,
	    socket_type socket,
	    tcp::endpoint remote_address,
	    std::string const & doc_root,
	    config const & conf)
	    : handlers_ (handlers)
	    , websocket_handlers_ (websocket_handlers)
	    , socket_ (std::move (socket))
	    , remote_address_ (remote_address)
	    , strand_ (socket_.get_executor ())
	    , timer_ (socket_.get_executor ().context (),
	          (std::chrono::steady_clock::time_point::max) ())
	    , doc_root_ (doc_root)
	    , config_ (conf)
	    , queue_ (*this)
	{
	}

	void write_json_response (std::string body)
	{
		http::response<http::string_body> res{ http::status::ok, client_version };
		res.set (http::field::server, "web/1");
		res.set (http::field::content_type, "application/json");
		res.keep_alive (client_keepalive);
		res.body () = body;
		res.prepare_payload ();
		queue_ (std::move (res));
	}

private:
	void run ()
	{
		on_timer ({});
		read ();
	}

	void read ()
	{
		// Set the timer, allowing for very long running requests
		timer_.expires_after (std::chrono::hours (1));

		// Make the request empty before reading, otherwise the operation behavior is undefined.
		req_ = {};

		// Read a request
		http::async_read (socket_, buffer_, req_,
		    boost::asio::bind_executor (
		        strand_,
		        std::bind (
		            &http_session::on_read,
		            shared_from_this (),
		            std::placeholders::_1)));
	}

	void on_timer (boost::system::error_code ec)
	{
		if (ec && ec != boost::asio::error::operation_aborted)
		{
			fail (ec, "timer");
		}
		else
		{
			// Verify that the timer really expired since the deadline may have moved.
			if (timer_.expiry () <= std::chrono::steady_clock::now ())
			{
				// Closing the socket cancels all outstanding operations. They
				// will complete with boost::asio::error::operation_aborted
				socket_.shutdown (tcp::socket::shutdown_both, ec);
				socket_.close (ec);
				return;
			}

			// Wait on the timer
			timer_.async_wait (
			    boost::asio::bind_executor (
			        strand_,
			        std::bind (
			            &http_session::on_timer,
			            shared_from_this (),
			            std::placeholders::_1)));
		}
	}

	void on_read (boost::system::error_code ec)
	{
		if (ec != boost::asio::error::operation_aborted)
		{
			// Did the peer close the connection?
			if (ec == http::error::end_of_stream)
			{
				return graceful_shutdown ();
			}

			if (ec)
			{
				return fail (ec, "read");
			}

			// See if it is a WebSocket Upgrade
			if (websocket::is_upgrade (req_))
			{
				std::shared_ptr<websocket_session> session;
				auto target_path = url (req_.base ().target ().to_string ()).path;
				std::cout << "WebSocket upgrade on path:" << target_path << std::endl;
				for (auto & handler : websocket_handlers_)
				{
					if (std::regex_match (target_path, handler.path))
					{
						std::vector<std::string> params;
						std::smatch match;
						std::regex_search (target_path, match, handler.path);
						{
							// Note that size() is number of matching groups +1 for the whole match (which is in m[0])
							for (int i = 1; i < match.size (); i++)
							{
								params.push_back (match[i]);
							}
						}
						session = std::make_shared<websocket_session> (std::move (socket_), handler);
						break;
					}
				}

				// Create a WebSocket websocket_session by transferring the socket
				if (session)
				{
					session->do_accept (std::move (req_));
				}
				else
				{
					std::cerr << "WebSocket connection denied, path is not a valid target: " << target_path << std::endl;
				}
			}
			else
			{
				// Send the response
				handle_request (remote_address_, doc_root_, std::move (req_), queue_);

				// If we aren't at the queue limit, try to pipeline another request
				if (!queue_.is_full ())
				{
					read ();
				}
			}
		}
	}

	void on_write (boost::system::error_code ec, bool close)
	{
		if (ec != boost::asio::error::operation_aborted)
		{
			if (!ec)
			{
				if (close)
				{
					return graceful_shutdown ();
				}

				// Inform the queue that a write completed
				if (queue_.on_write ())
				{
					// Read another request
					read ();
				}
			}
			else
			{
				return fail (ec, "write");
			}
		}
	}

	void graceful_shutdown ()
	{
		boost::system::error_code ec;
		socket_.shutdown (tcp::socket::shutdown_send, ec);
	}

	template <class Body, class Allocator, class Send>
	void handle_request (tcp::endpoint remote_address, boost::beast::string_view doc_root, http::request<Body, http::basic_fields<Allocator>> && req, Send && send)
	{
		client_version = req.version ();
		client_keepalive = req.keep_alive ();

		// Returns a bad request response
		auto const bad_request =
		    [&req](boost::beast::string_view why) {
			    http::response<http::string_body> res{ http::status::bad_request, req.version () };
			    res.set (http::field::server, "web/1");
			    res.set (http::field::content_type, "text/html");
			    res.keep_alive (req.keep_alive ());
			    res.body () = why.to_string ();
			    res.prepare_payload ();
			    return res;
		    };

		// Returns a not found response
		auto const not_found =
		    [&req](boost::beast::string_view target) {
			    http::response<http::string_body> res{ http::status::not_found, req.version () };
			    res.set (http::field::server, "web/1");
			    res.set (http::field::content_type, "text/html");
			    res.keep_alive (req.keep_alive ());
			    res.body () = "The resource '" + target.to_string () + "' was not found.";
			    res.prepare_payload ();
			    return res;
		    };

		// Returns a server error response
		auto const server_error =
		    [&req](boost::beast::string_view what) {
			    http::response<http::string_body> res{ http::status::internal_server_error, req.version () };
			    res.set (http::field::server, "web/1");
			    res.set (http::field::content_type, "text/html");
			    res.keep_alive (req.keep_alive ());
			    res.body () = "An error occurred: '" + what.to_string () + "'";
			    res.prepare_payload ();
			    return res;
		    };

		// Request path must be absolute and not contain "..".
		if (req.target ().empty () || req.target ()[0] != '/' || req.target ().find ("..") != boost::beast::string_view::npos)
		{
			return send (bad_request ("Illegal request-target"));
		}

		auto target_path (req.target ().to_string ());
		for (auto & handler : handlers_)
		{
			if (std::regex_match (target_path, handler.path) && handler.verb == req.method ())
			{
				std::vector<std::string> params;
				std::smatch match;
				std::regex_search (target_path, match, handler.path);
				{
					// Note that size() is number of matching groups +1 for the whole match (which is in m[0])
					for (int i = 1; i < match.size (); i++)
					{
						params.push_back (match[i]);
					}
				}
				handler.handler (req.body (), params, shared_from_this ());
				return;
			}
		}

		if (!config_.static_pages_allow || (!config_.static_pages_allow_remote && !remote_address.address ().is_loopback ()))
		{
			return send (bad_request ("Access denied"));
		}

		// Build the path to the requested file
		std::string path = path_cat (doc_root, req.target ());
		if (req.target ().back () == '/')
		{
			path.append ("index.html");
		}

		// Attempt to open the file
		boost::beast::error_code ec;
		http::file_body::value_type body;
		body.open (path.c_str (), boost::beast::file_mode::scan, ec);

		if (ec == boost::system::errc::no_such_file_or_directory)
		{
			// We handle single-page application by rewriting non-existing files to index.html
			path = path_cat (doc_root, "/index.html");
			body.open (path.c_str (), boost::beast::file_mode::scan, ec);
			if (ec == boost::system::errc::no_such_file_or_directory)
			{
				return send (not_found (req.target ()));
			}
		}

		if (ec)
		{
			return send (server_error (ec.message ()));
		}

		// Cache the size since we need it after the move
		auto const size = body.size ();

		// Respond to HEAD request
		if (req.method () == http::verb::head)
		{
			http::response<http::empty_body> res{ http::status::ok, req.version () };
			res.set (http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set (http::field::content_type, mime_type (path));
			res.content_length (size);
			res.keep_alive (req.keep_alive ());
			return send (std::move (res));
		}

		// Respond to GET request
		http::response<http::file_body> res{
			std::piecewise_construct,
			std::make_tuple (std::move (body)),
			std::make_tuple (http::status::ok, req.version ())
		};
		res.set (http::field::server, BOOST_BEAST_VERSION_STRING);
		res.set (http::field::content_type, mime_type (path));
		res.content_length (size);
		res.keep_alive (req.keep_alive ());

		return send (std::move (res));
	}
};

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
	tcp::acceptor acceptor_;
	socket_type socket_;
	std::string const & doc_root_;
	std::vector<rest_endpoint> const & handlers_;
	std::vector<websocket_endpoint> const & websocket_handlers_;
	config const & config_;

public:
	listener (boost::asio::io_context & ioc,
	    tcp::endpoint endpoint,
	    std::string const & doc_root,
	    std::vector<rest_endpoint> const & handlers,
	    std::vector<websocket_endpoint> const & websocket_handlers,
	    config const & conf)
	    : acceptor_ (ioc)
	    , socket_ (ioc)
	    , doc_root_ (doc_root)
	    , handlers_ (handlers)
	    , websocket_handlers_ (websocket_handlers)
	    , config_ (conf)
	{
		boost::system::error_code ec;

		// Open the acceptor
		acceptor_.open (endpoint.protocol (), ec);
		if (ec)
		{
			fail (ec, "open");
			return;
		}

		// Allow address reuse
		acceptor_.set_option (boost::asio::socket_base::reuse_address (true));
		if (ec)
		{
			fail (ec, "set_option");
			return;
		}

		// Bind to the server address
		acceptor_.bind (endpoint, ec);
		if (ec)
		{
			fail (ec, "bind");
			return;
		}

		// Start listening for connections
		acceptor_.listen (boost::asio::socket_base::max_listen_connections, ec);
		if (ec)
		{
			fail (ec, "listen");
		}
	}

	// Start accepting incoming connections
	void run ()
	{
		if (acceptor_.is_open ())
		{
			do_accept ();
		}
	}

	void do_accept ()
	{
		acceptor_.async_accept (
		    socket_,
		    std::bind (
		        &listener::on_accept,
		        shared_from_this (),
		        std::placeholders::_1));
	}

	void on_accept (boost::system::error_code ec)
	{
		if (ec)
		{
			fail (ec, "accept");
			std::this_thread::sleep_for (std::chrono::seconds (1));
		}
		else
		{
			// Create the http_session and run it
			std::make_shared<http_session> (handlers_, websocket_handlers_, std::move (socket_), socket_.remote_endpoint (), doc_root_, config_)->run ();
		}

		// Accept another connection
		do_accept ();
	}
};

/** Web server with WebSocket support and REST endpoints */
class webserver
{
public:
	webserver (config const & conf, unsigned int thread_count = std::thread::hardware_concurrency ());
	void start (std::string address, uint16_t port, std::string doc_root);
	void stop ();
	/** Called before start to register REST endpoints */
	void add_endpoint (http::verb verb, std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler);
	/** Called before start to register REST GET endpoints */
	void add_get_endpoint (std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler);
	/** Called before start to register REST POST endpoints */
	void add_post_endpoint (std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler);
	/** Called before start to register REST DELETE endpoints */
	void add_delete_endpoint (std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler);
	/** Called before start to register WebSocket endpoints */
	void add_websocket_endpoint (std::string path, std::function<void(std::string, std::shared_ptr<websocket_session>)> handler);

private:
	config config_;
	boost::asio::io_context ioc;
	std::vector<std::thread> threads;
	std::vector<rest_endpoint> handlers;
	std::vector<websocket_endpoint> websocket_handlers;
};

webserver::webserver (config const & conf, unsigned int thread_count)
    : config_ (conf)
    , ioc (thread_count)
{
	threads.reserve (thread_count - 1);
}

void webserver::add_endpoint (http::verb verb, std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler)
{
	std::string regex_final ("^");
	regex_final += std::regex_replace (path, std::regex ("\\?"), "([^\\\?]+)");
	regex_final += "$";

	handlers.emplace_back (verb, path, std::regex (regex_final), handler);
}

void webserver::add_get_endpoint (std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler)
{
	add_endpoint (http::verb::get, path, handler);
}

void webserver::add_post_endpoint (std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler)
{
	add_endpoint (http::verb::post, path, handler);
}

void webserver::add_delete_endpoint (std::string path, std::function<void(std::string, std::vector<std::string>, std::shared_ptr<http_session>)> handler)
{
	add_endpoint (http::verb::delete_, path, handler);
}

void webserver::add_websocket_endpoint (std::string path, std::function<void(std::string, std::shared_ptr<websocket_session>)> handler)
{
	std::string regex_final ("^");
	regex_final += std::regex_replace (path, std::regex ("\\?"), "([^\\\?]+)");
	regex_final += "$";

	websocket_handlers.emplace_back (path, std::regex (regex_final), handler);
}

void webserver::start (std::string address_a, uint16_t port_a, std::string doc_root_a)
{
	auto const address = boost::asio::ip::make_address (address_a);
	std::make_shared<listener> (ioc, tcp::endpoint{ address, port_a }, doc_root_a, handlers, websocket_handlers, config_)->run ();

	// Capture SIGINT and SIGTERM to perform a clean shutdown
	boost::asio::signal_set signals (ioc, SIGINT, SIGTERM);
	signals.async_wait ([&](boost::system::error_code const &, int) {
		ioc.stop ();
	});

	// Run the I/O service on the requested number of threads
	for (auto i = threads.capacity () - 1; i > 0; --i)
	{
		threads.emplace_back ([this] {
			ioc.run ();
		});
	}
	ioc.run ();

	// Block until all the threads exit via sigint/sigterm or stop ()
	for (auto & t : threads)
	{
		t.join ();
	}
}

void webserver::stop ()
{
	ioc.stop ();
}
}
