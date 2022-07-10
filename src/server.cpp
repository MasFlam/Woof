#include "internal.hpp"

namespace woof {

Server::Server()
:
	m(std::make_shared<ServerState>(
		default_log,
		"0.0.0.0",
		8042,
		std::make_shared<RouterNode>()
	))
{}

Server &
Server::logger(const LogHandler &handler)
{
	m->logger = handler;
	return *this;
}

Server &
Server::address(const std::string &address)
{
	m->address = address;
	return *this;
}

Server &
Server::port(int port)
{
	m->port = port;
	return *this;
}

static inline std::shared_ptr<RouterNode>
resolve_pattern(std::shared_ptr<ServerState> m, const PathPattern &path, std::vector<std::pair<int, std::string>> &path_param_names)
{
	std::shared_ptr<RouterNode> node = m->router;
	for (int i = 0; auto &segment : path.segments) {
		if (segment.wildcard) {
			if (!segment.name.empty()) path_param_names.emplace_back(i, segment.name);
			if (!node->wildcard) {
				node->wildcard = std::make_shared<RouterNode>(node);
			}
			node = node->wildcard;
		} else {
			if (!node->subpaths.contains(segment.name)) {
				node->subpaths[segment.name] = std::make_shared<RouterNode>(node);
			}
			node = node->subpaths[segment.name];
		}
		++i;
	}
	return node;
}

void
Server::add_endpoint(Method method, const PathPattern &path, const RequestHandler &handler)
{
	std::vector<std::pair<int, std::string>> path_param_names;
	std::shared_ptr<RouterNode> node = resolve_pattern(m, path, path_param_names);
	//std::vector<std::string> path_param_names;
	//size_t segment_start = path[0] == '/';
	//for (size_t i = segment_start; i < path.size() + !(path.back() == '/'); ++i) {
	//	if (path[i] == '/' || path[i] == '\0') {
	//		std::string segment(path, segment_start, i - segment_start);
	//		segment_start = i+1;
	//		if (segment[0] == '{') {
	//			if (segment.back() != '}') {
	//				// throw
	//			}
	//			path_param_names.emplace_back(segment, 1, segment.size() - 2);
	//			if (!node->wildcard) {
	//				node->wildcard = std::make_shared<RouterNode>();
	//			}
	//			node = node->wildcard;
	//		} else {
	//			if (!node->subpaths.contains(segment)) {
	//				node->subpaths[segment] = std::make_shared<RouterNode>();
	//			}
	//			node = node->subpaths[segment];
	//		}
	//	}
	//}
	auto &map = path.suffix_wildcard ? node->globstar_handlers : node->handlers;
	if (map.contains(method)) {
		// TODO: error on endpoint redefinition
	}
	path_param_names.shrink_to_fit();
	map[method] = {std::move(path_param_names), handler};
}

void
Server::do_add_middleware(size_t hash, const PathPattern &path, const MiddlewareCreator &mw)
{
	std::vector<std::pair<int, std::string>> path_param_names;
	for (int i = 0; auto &segment : path.segments) {
		if (segment.wildcard && !segment.name.empty()) {
			path_param_names.emplace_back(i, segment.name);
		}
	}
	m->mw_map[hash] = {mw, path, std::move(path_param_names), int(m->mw_list.size())};
	m->mw_list.push_back(hash);
}

} // namespace woof
 