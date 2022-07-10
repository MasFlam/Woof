#include "internal.hpp"

namespace woof {

std::unordered_map<std::string, std::string> &
Request::Path::map()
{
	return m->path_params;
}

const std::unordered_map<std::string, std::string> &
Request::Path::map() const
{
	return m->path_params;
}

const std::string &
Request::Path::string() const
{
	return m->target.path;
}

const std::string &
Request::Path::string_raw() const
{
	return m->path_string_raw;
}

std::unordered_multimap<std::string, std::string> &
Request::Query::map()
{
	return m->query_params;
}

const std::unordered_multimap<std::string, std::string> &
Request::Query::map() const
{
	return m->query_params;
}

const std::string &
Request::Query::string() const
{
	return m->target.query;
}

const std::string &
Request::Query::string_raw() const
{
	return m->query_string_raw;
}

std::iostream &
Request::Body::stream()
{
	return m->request_body_stream;
}

const std::iostream &
Request::Body::stream() const
{
	return m->request_body_stream;
}

Method
Request::method() const
{
	return m->method;
}

const std::string &
Request::target_string() const
{
	return m->target.decoded;
}

const std::string &
Request::target_string_raw() const
{
	return m->target_string_raw;
}

HeaderMap &
Request::headers()
{
	return m->headers;
}

const HeaderMap &
Request::headers() const
{
	return m->headers;
}

MiddlewareI &
Request::do_middleware(size_t hash) const
{
	return *m->mw_map[hash];
}

}
