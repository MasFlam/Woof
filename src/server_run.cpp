#include "internal.hpp"
#include <boost/asio.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/beast.hpp>
#include <algorithm>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;

namespace woof {

inline constexpr Method
http_method(http::verb verb)
{
	switch (verb) {
		case http::verb::get:     return Method::GET;
		case http::verb::head:    return Method::HEAD;
		case http::verb::post:    return Method::POST;
		case http::verb::put:     return Method::PUT;
		case http::verb::delete_: return Method::DELETE;
		case http::verb::connect: return Method::CONNECT;
		case http::verb::options: return Method::OPTIONS;
		case http::verb::trace:   return Method::TRACE;
		case http::verb::patch:   return Method::PATCH;
		default: return Method::UNKNOWN;
	}
}

static inline std::shared_ptr<RouterNode>
dfs_route(bool &globstar, Method method, std::shared_ptr<RouterNode> node, std::vector<std::string>::iterator it, std::vector<std::string>::iterator end)
{
	if (it == end) {
		if (node->handlers.contains(method)) {
			return node;
		} else {
			return {};
		}
	}
	for (auto &subpath : node->subpaths) {
		if (subpath.first == *it) {
			auto sp = dfs_route(globstar, method, subpath.second, std::next(it), end);
			if (sp) return sp;
		}
	}
	if (node->wildcard) {
		auto sp = dfs_route(globstar, method, node->wildcard, std::next(it), end);
		if (sp) return sp;
	}
	if (node->globstar_handlers.contains(method)) {
		globstar = true;
		return node;
	}
	return {};
}

/*static inline void
list_middlewares(std::shared_ptr<RouterNode> node, std::vector<RouterNode::MiddlewareHandler *> &mw_list)
{
	for (auto it = node->mw_list.rbegin(); it != node->mw_list.rend(); ++it) {
		if (!it->globstar) {
			mw_list.push_back(&*it);
		}
	}
	for (auto nd = node; nd; nd = nd->parent.lock()) {
		for (auto it = nd->mw_list.rbegin(); it != nd->mw_list.rend(); ++it) {
			if (it->globstar) {
				mw_list.push_back(&*it);
			}
		}
	}
	std::reverse(mw_list.begin(), mw_list.end());
}*/

static inline void
handle_connection(std::shared_ptr<ServerState> m, std::shared_ptr<tcp::socket> socket)
{ // TODO: general error handling here
	beast::tcp_stream stream(std::move(*socket));
	beast::flat_buffer buffer;
	
	auto state = std::make_shared<ConnectionState>();
	state->status_code = 200;
	
	auto respond = [&stream](Status status) {
		http::response<http::empty_body> beast_response;
		beast_response.version(11);
		beast_response.result(status);
		beast_response.prepare_payload();
		http::write(stream, beast_response);
		stream.close();
	};
	
	// 1. Request head
	// TODO: request size limits
	http::request_parser<http::empty_body> head_parser;
	http::read_header(stream, buffer, head_parser);
	auto &head = head_parser.get();
	
	// 1.1. Request method
	state->method = http_method(head.method());
	
	// 1.2. Parse the target string
	state->target_string_raw = head.target();
	state->target = ParsedTarget(state->target_string_raw);
	if (!state->target.success) {
		// Invalid target string => 400
		respond(400);
		return;
	}
	state->path_string_raw = state->target.path_raw;
	state->query_string_raw = state->target.query_raw;
	// TODO: handling trailing slash quirks all around the library
	if (state->target.path_segments.back().empty()) state->target.path_segments.pop_back();
	
	// 1.3. Route the request
	bool globstar = false;
	std::shared_ptr<RouterNode> node = dfs_route(
		globstar,
		state->method,
		m->router,
		state->target.path_segments.begin(),
		state->target.path_segments.end()
	);
	if (!node) {
		// Handler not found => 404
		respond(404);
		return;
	}
	const RouterNode::Handler &handler = (globstar ? node->globstar_handlers : node->handlers)[state->method];
	
	// 1.4. Resolve the list of middlewares in correct order
	std::vector<std::pair<size_t, ServerState::MiddlewareConfig *>> mw_list;
	const int nreal = state->target.path_segments.size();
	for (size_t hash : m->mw_list) {
		auto &mwc = m->mw_map[hash];
		const int nsegments = mwc.path.segments.size();
		if (nreal < nsegments) continue;
		for (int i = 0; i < nsegments; ++i) {
			auto &real_segment = state->target.path_segments[i];
			auto &segment = mwc.path.segments[i];
			if (!segment.wildcard && real_segment != segment.name) {
				goto no_match;
			}
		}
		if (mwc.path.suffix_wildcard || nreal == nsegments) {
			mw_list.emplace_back(hash, &mwc);
		}
		no_match:;
	}
	std::sort(mw_list.begin(), mw_list.end(), [&m, nreal](const auto &a, const auto &b) {
		ServerState::MiddlewareConfig &ac = m->mw_map[a.first];
		ServerState::MiddlewareConfig &bc = m->mw_map[b.first];
		int asz = ac.path.segments.size();
		int bsz = bc.path.segments.size();
		if ((asz == nreal) > (bsz == nreal)) return true;
		if ((asz == nreal) < (bsz == nreal)) return false;
		if (asz < bsz) return true;
		if (asz > bsz) return false;
		return ac.idx < bc.idx;
	});
	
	// 1.5. Path params
	std::unordered_map<std::string, std::string> endpoint_path_params;
	for (auto &p : handler.path_params) {
		endpoint_path_params[p.second] = state->target.path_segments[p.first];
	}
	
	// 1.6. Query params
	for (auto &p : state->target.query_params) {
		state->query_params.emplace(std::move(p.first), std::move(p.second));
	}
	
	// 1.7. Request headers
	for (auto &field : head) {
		state->headers.emplace(field.name_string(), field.value());
	}
	
	// 2. Request body
	
	http::request_parser<http::string_body> parser(std::move(head_parser));
	http::read(stream, buffer, parser);
	auto full_request = parser.release();
	
	state->request_body_stream = std::stringstream(std::move(full_request.body()));
	
	// 3. Call the handler
	
	Request request(state);
	Response response(state);
	
	// 3.1. Instantiate the middlewares and assemble their path param maps
	std::vector<std::pair<MiddlewareI *, std::unordered_map<std::string, std::string>>> mws;
	mws.reserve(mw_list.size());
	for (auto &[hash, mwc] : mw_list) {
		std::unordered_map<std::string, std::string> path_params;
		for (auto &p : mwc->path_params) {
			path_params[p.second] = state->target.path_segments[p.first];
		}
		MiddlewareI *mw = mwc->creator();
		state->mw_map[hash] = mw;
		mws.emplace_back(mw, std::move(path_params));
	}
	
	// 3.2. Call MiddlewareI::before on middlewares
	for (auto &mw : mws) {
		state->path_params = std::move(mw.second);
		mw.first->before(request, response);
		mw.second = std::move(state->path_params);
	}
	
	// 3.3. Call the endpoint handler
	try {
		state->path_params = std::move(endpoint_path_params);
		handler.handler(request, response);
	} catch (...) {
		// TODO: handle endpoint handler exception
	}
	
	// 3.4. Call MiddlewareI::after on middlewares
	for (auto it = mws.rbegin(); it != mws.rend(); ++it) {
		state->path_params = std::move(it->second);
		it->first->after(request, response);
	}
	
	// 3.5. Delete middlewares
	for (auto [hash, mw] : state->mw_map) {
		delete mw;
	}
	
	// 4. Write the response
	
	http::response<http::string_body> beast_response;
	beast_response.version(11);
	beast_response.result(state->status_code);
	// TODO: setting content-type design decisions, etc
	beast_response.insert("Content-Type", "text/plain; charset=utf-8");
	beast_response.body() = std::move(state->response_body_stream).str();
	
	for (auto &[name, value] : state->response_headers) {
		beast_response.insert(name, value);
	}
	
	beast_response.prepare_payload();
	
	http::write(stream, beast_response);
	stream.close();
}

void
Server::run(int nworkers)
{
	asio::io_context ioc(nworkers + 1);
	
	asio::signal_set signal_set(ioc, SIGINT, SIGTERM);
	signal_set.async_wait([&ioc, this] (error_code, int signal) {
		m->info("Server stopping: Received signal " + std::to_string(signal));
		ioc.stop();
	});
	
	tcp::acceptor acceptor(ioc);
	tcp::endpoint endpoint(asio::ip::make_address(m->address), m->port);
	acceptor.open(endpoint.protocol());
	acceptor.bind(endpoint);
	acceptor.listen();
	
	/*
	auto handler_lambda = [](std::shared_ptr<tcp::socket> socket) {
		beast::tcp_stream stream(std::move(*socket));
		beast::flat_buffer buffer;
		
		http::request_parser<http::empty_body> head_parser;
		http::read_header(stream, buffer, head_parser);
		auto &head = head_parser.get();
		
		auto state = std::make_shared<ConnectionState>();
		
		// 1. Request head
		
		// 1.1. Request method
		state->method = http_method(head.method());
		
		// 1.2. Parse the target string
		state->target_string_raw = head.target();
		state->target = ParsedTarget(state->target_string_raw);
		if (!state->target.success) {
			// TODO: invalid target string, 400 bad request
		}
		state->path_string_raw = state->target.path_raw;
		state->query_string_raw = state->target.query_raw;
		
		//std::cout << std::quoted(state->target.path) << " ? " << std::quoted(state->target.query) << '\n';
		//std::cout << state->target_string_raw << '\n';
		//for (auto &segment : state->target.path_segments) {
		//	std::cout << "segment: " << std::quoted(segment) << '\n';
		//}
		//for (auto &[key, val] : state->target.query_params) {
		//	std::cout << "query param: " << std::quoted(key) << " = " << std::quoted(val) << '\n';
		//}
		
		// 1.3. Look up the RouterNode
		std::function<std::shared_ptr<RouterNode>(const std::shared_ptr<RouterNode> &node, int i)> route =
			[&state, &route](std::shared_ptr<RouterNode> &node, int i) {
				if (i == state->target.path_segments.size()) {
					
				}
				for (auto &[subsegment, subnode] : node->subpaths) {
					if (subsegment == state->target.path_segments[i]) {
						auto sp = route(subnode, i+1);
						if (sp) return sp;
					}
				}
				if (node->wildcard) {
					auto sp = route(node->wildcard, i+1);
					if (sp) return sp;
				}
				if (node->globstar_handlers.contains(state->method)) {
					
				}
			};
		
		std::shared_ptr<RouterNode> node = m->router;
		std::shared_ptr<RouterNode> last_globstar_node;
		std::vector<std::string_view> path_params;
		for (auto &segment : state->target.path_segments) {
			if (node->globstar_handlers.contains(state->method)) {
				last_globstar_node = node;
			}
			for (auto &[subsegment, subnode] : node->subpaths) {
				if (subsegment == segment) {
					node = subnode;
					goto next;
				}
			}
			if (node->wildcard) {
				node = node->wildcard;
				path_params.push_back(segment);
			}
			if (!node->wildcard) {
				// TODO: Handler not found, respond with 404
				http::response<http::string_body> r;
				r.version(11);
				r.result(404);
				http::write(stream, r);
				stream.close();
				return;
			}
			node = node->wildcard;
			path_params.push_back(segment);
			next:;
		}
		if (!node->method_handlers.contains(state->method)) {
			// TODO: bad method
		}
		
		// 1.4. Path params
		for (size_t i = 0; i < path_params.size(); ++i) {
			state->path_params[node->path_param_names[i]] = path_params[i]; // TODO: percent-encoding decoding
		}
		
		// 1.5. Query params
		for (auto &[key, value] : state->target.query_params) {
			state->query_params.emplace(key, value);
		}
		
		// 1.6. Put the request headers into a case-insensitive std::unordered_multimap
		for (auto &field : head) {
			state->headers.emplace(
				field.name_string(),
				field.value()
			);
		}
		
		// 2. Request body
		
		http::request_parser<http::string_body> parser(std::move(head_parser));
		http::read(stream, buffer, parser);
		auto full_request = parser.release();
		
		state->request_body_stream = std::stringstream(std::move(full_request.body()));
		
		// 3. Call the handler
		
		Request request;
		request.m = state;
		request.m_path.m = state;
		request.m_query.m = state;
		request.m_body.m = state;
		
		Response response;
		response.m = state;
		response.m_body.m = state;
		
		size_t nmws = m->mw_list.size();
		
		for (size_t i = 0; i < nmws; ++i) {
			size_t hash = m->mw_list[i];
			MiddlewareI *mw = m->mw_map[hash]();
			state->mw_map[hash] = mw;
		}
		
		for (size_t hash : m->mw_list) {
			state->mw_map[hash]->before(request, response);
		}
		
		try {
			node->method_handlers[state->method](request, response);
		} catch (const Status &status) {
			state->status_code = status;
		}
		
		for (int i = nmws - 1; i >= 0; --i) {
			state->mw_map[m->mw_list[i]]->after(request, response);
		}
		
		for (auto [hash, mw] : state->mw_map) {
			delete mw;
		}
		
		// 4. Write the response
		
		http::response<http::string_body> beast_response;
		beast_response.version(11);
		beast_response.result(state->status_code);
		beast_response.insert("Content-Type", "text/plain; charset=utf-8");
		beast_response.body() = std::move(state->response_body_stream).str();
		
		for (auto &[name, value] : state->response_headers) {
			beast_response.insert(name, value);
		}
		
		beast_response.prepare_payload();
		
		http::write(stream, beast_response);
		stream.close();
	}; // handler_lambda
	*/
	
	// It needs to take error_code ~~in order for the use_future completion token to work~~
	// It just fucking doesn't compile without it, thanks for not telling me in the documentation
	asio::experimental::concurrent_channel<void(error_code, std::shared_ptr<tcp::socket>)> jobs(ioc, 3*nworkers);
	
	std::function<void(error_code, std::shared_ptr<tcp::socket>)> worker_fn =
		[this, &jobs, &worker_fn](error_code, std::shared_ptr<tcp::socket> sp) {
			handle_connection(m, sp);
			jobs.async_receive(worker_fn);
		};
	
	std::vector<std::thread> workers(nworkers);
	for (int i = 0; i < nworkers; ++i) {
		workers[i] = std::thread(
			[this, &ioc, &jobs, &worker_fn, i]() {
				jobs.async_receive(worker_fn);
				ioc.run();
				m->info("Worker " + std::to_string(i) + " stopped");
			}
		);
	}
	
	std::function<void(error_code, tcp::socket)> accept_fn =
		[this, &ioc, &acceptor, &jobs, &accept_fn](error_code, tcp::socket socket) {
			jobs.try_send(error_code(), std::make_shared<tcp::socket>(std::move(socket)));
			acceptor.async_accept(ioc, accept_fn);
		};
	
	acceptor.async_accept(ioc, accept_fn);
	
	{
		char startup_message[200];
		std::string address_string = endpoint.address().to_string();
		snprintf(startup_message, sizeof(startup_message),
			"Started Woof %s HTTP server at %s:%d on %d worker threads",
			VERSION, address_string.c_str(), int(endpoint.port()), nworkers
		);
		m->info(startup_message);
	}
	
	ioc.run();
	m->info("Main server thread finished work");
	for (std::thread &worker : workers) {
		worker.join();
	}
}

}
