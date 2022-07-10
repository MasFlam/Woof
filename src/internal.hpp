#ifndef _WOOF_internal_hpp
#define _WOOF_internal_hpp

#include <cstring>
#include <sstream>
#include <string_view>
#include <vector>
#include <woof/woof.hpp>

namespace woof {

struct ParsedTarget {
	bool success;
	std::string decoded;
	std::string_view path_raw;
	std::string path;
	std::vector<std::string> path_segments;
	std::string_view query_raw;
	std::string query;
	std::vector<std::pair<std::string, std::string>> query_params;
	
	ParsedTarget() = default;
	ParsedTarget(const std::string_view &sv);
};

struct ConnectionState {
	Method method;
	ParsedTarget target;
	std::string target_string_raw;
	std::string path_string_raw;
	std::string query_string_raw;
	std::unordered_map<std::string, std::string> path_params;
	std::unordered_multimap<std::string, std::string> query_params;
	HeaderMap headers;
	HeaderMap response_headers;
	std::stringstream request_body_stream;
	std::stringstream response_body_stream;
	Status status_code;
	std::unordered_map<size_t, MiddlewareI *> mw_map;
};

struct RouterNode {
	struct Handler {
		std::vector<std::pair<int, std::string>> path_params;
		RequestHandler handler;
	};
	
	std::weak_ptr<RouterNode> parent;
	std::unordered_map<std::string, std::shared_ptr<RouterNode>> subpaths;
	std::shared_ptr<RouterNode> wildcard;
	std::unordered_map<Method, Handler> handlers;
	std::unordered_map<Method, Handler> globstar_handlers;
	
	RouterNode(std::shared_ptr<RouterNode> parent_ = {}) : parent(parent_) {}
};

struct ServerState {
	struct MiddlewareConfig {
		MiddlewareCreator creator;
		PathPattern path;
		std::vector<std::pair<int, std::string>> path_params;
		int idx;
	};
	
	LogHandler logger;
	std::string address;
	int port;
	std::shared_ptr<RouterNode> router;
	std::unordered_map<size_t, MiddlewareConfig> mw_map;
	std::vector<size_t> mw_list;
	
	void log(LogLevel level, const char *s) const        { logger(level, s, strlen(s)); }
	void log(LogLevel level, const std::string &s) const { logger(level, s.c_str(), s.size()); }
	
	void trace(const char *s) const    { logger(LogLevel::TRACE,    s, strlen(s)); }
	void debug(const char *s) const    { logger(LogLevel::DEBUG,    s, strlen(s)); }
	void info(const char *s) const     { logger(LogLevel::INFO,     s, strlen(s)); }
	void warn(const char *s) const     { logger(LogLevel::WARN,     s, strlen(s)); }
	void error(const char *s) const    { logger(LogLevel::ERROR,    s, strlen(s)); }
	void critical(const char *s) const { logger(LogLevel::CRITICAL, s, strlen(s)); }
	
	void trace(const std::string &s) const    { logger(LogLevel::TRACE,    s.c_str(), s.size()); }
	void debug(const std::string &s) const    { logger(LogLevel::DEBUG,    s.c_str(), s.size()); }
	void info(const std::string &s) const     { logger(LogLevel::INFO,     s.c_str(), s.size()); }
	void warn(const std::string &s) const     { logger(LogLevel::WARN,     s.c_str(), s.size()); }
	void error(const std::string &s) const    { logger(LogLevel::ERROR,    s.c_str(), s.size()); }
	void critical(const std::string &s) const { logger(LogLevel::CRITICAL, s.c_str(), s.size()); }
};

void default_log(LogLevel level, const char *str, size_t len);

}

#endif
