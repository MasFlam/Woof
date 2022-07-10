#include "internal.hpp"

namespace woof {

std::iostream &
Response::Body::stream()
{
	return m->response_body_stream;
}

const std::iostream &
Response::Body::stream() const
{
	return m->response_body_stream;
}

Status
Response::status() const
{
	return m->status_code;
}

void
Response::status(Status status)
{
	m->status_code = status;
}

HeaderMap &
Response::headers()
{
	return m->response_headers;
}

const HeaderMap &
Response::headers() const
{
	return m->response_headers;
}

}
