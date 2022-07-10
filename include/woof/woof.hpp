#ifndef _WOOF_woof_woof_hpp
#define _WOOF_woof_woof_hpp

#include <cctype>
#include <charconv>
#include <functional>
#include <istream>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if __cplusplus < 202002L
#error "Woof requires C++20 or newer"
#else

namespace woof {

constexpr const int VERSION_MAJOR = 0;
constexpr const int VERSION_MINOR = 1;
constexpr const int VERSION_PATCH = 0;
constexpr const char VERSION[] = "0.1.0";

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL };
enum class Method { UNKNOWN, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH };

class ConnectionState;
class MiddlewareI;
class Request;
class Response;
class Server;
class ServerState;

template<class T>
struct StringConverter;

using LogHandler = std::function<void(LogLevel, const char *, size_t)>;
using RequestHandler = std::function<void(Request &, Response &)>;
using MiddlewareCreator = std::function<MiddlewareI *()>;

class MiddlewareI {
public:
	virtual ~MiddlewareI() = default;
	virtual void before(Request &, Response &) = 0;
	virtual void after(Request &, Response &) = 0;
};

// TODO: deinline these two to eliminate some #includes

// Hash with 64bit FNV1a
struct CaseInsensitiveHash {
	size_t operator()(const std::string &s) const {
		uint64_t hash = 0xcbf29ce484222325;
		const uint64_t prime = 0x100000001b3;
		for (char c : s) {
			hash ^= std::toupper(c);
			hash *= prime;
		}
		return hash;
	}
};

struct CaseInsensitiveEquals {
	bool operator()(const std::string &a, const std::string &b) const {
		size_t len = a.size();
		if (len != b.size()) return false;
		for (size_t i = 0; i < len; ++i) {
			if (std::toupper(a[i]) != std::toupper(b[i])) return false;
		}
		return true;
	}
};

using HeaderMap = std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEquals>;

template<size_t N>
struct StringConstant {
	static constexpr size_t length = N-1;
	char chars[N];
	
	constexpr
	StringConstant() noexcept
	{
		for (int i = 0; i < N; ++i) {
			chars[i] = '\0';
		}
	}
	
	constexpr
	StringConstant(const char (&chars_)[N]) noexcept
	{
		for (int i = 0; i < N; ++i) {
			chars[i] = chars_[i];
		}
	}
	
	template<size_t idx, size_t len>
	constexpr StringConstant<len+1>
	substr() const noexcept
	{
		StringConstant<len+1> sc;
		for (size_t i = 0; i < len; ++i) {
			sc.chars[i] = chars[idx + i];
		}
		sc.chars[len] = '\0';
		return sc;
	}
	
	constexpr char
	operator[](size_t idx) const noexcept
	{
		return chars[idx];
	}
	
	constexpr
	operator std::string() const noexcept
	{
		return chars;
	}
};

template<StringConstant sc, size_t idx>
struct _path;

struct PathPattern {
	struct Segment {
		bool wildcard;
		std::string name;
	};
	
	std::vector<Segment> segments {};
	bool suffix_wildcard = false;
	
	static PathPattern make(const std::string &pattern);
	
	template<StringConstant pattern>
	static constexpr PathPattern
	make() noexcept
	{
		PathPattern pp;
		_path<pattern, pattern.chars[0] == '/'>::path(pp);
		return pp;
	}
};

#include <woof/path_pattern_sfinae.hpp>

class Status {
public:
	
	int code;
	
	constexpr
	Status(int code_ = 200) noexcept
	:
		code(code_)
	{}
	
	constexpr
	operator int() noexcept
	{ return code; }
	
}; // class Status

namespace status {
	inline constexpr Status CONTINUE = 100;
	inline constexpr Status SWITCHING_PROTOCOLS = 101;
	inline constexpr Status PROCESSING = 102;
	inline constexpr Status EARLY_HINTS = 103;
	
	inline constexpr Status OK = 200;
	inline constexpr Status CREATED = 201;
	inline constexpr Status ACCEPTED = 202;
	inline constexpr Status NON_AUTHORITATIVE_INFORMATION = 203;
	inline constexpr Status NO_CONTENT = 204;
	inline constexpr Status RESET_CONTENT = 205;
	inline constexpr Status PARTIAL_CONTENT = 206;
	inline constexpr Status MULTI_STATUS = 207;
	inline constexpr Status ALREADY_REPORTED = 208;
	inline constexpr Status IM_USED = 226;
	
	inline constexpr Status MULTIPLE_CHOICES = 300;
	inline constexpr Status MOVED_PERMANENTLY = 301;
	inline constexpr Status FOUND = 302;
	inline constexpr Status SEE_OTHER = 303;
	inline constexpr Status NOT_MODIFIED = 304;
	inline constexpr Status USE_PROXY = 305;
	inline constexpr Status SWITCH_PROXY = 306;
	inline constexpr Status TEMPORARY_REDIRECT = 307;
	inline constexpr Status PERMANENT_REDIRECT = 308;
	
	inline constexpr Status BAD_REQUEST = 400;
	inline constexpr Status UNAUTHORIZED = 401;
	inline constexpr Status PAYMENT_REQUIRED = 402;
	inline constexpr Status FORBIDDEN = 403;
	inline constexpr Status NOT_FOUND = 404;
	inline constexpr Status METHOD_NOT_ALLOWED = 405;
	inline constexpr Status NOT_ACCEPTABLE = 406;
	inline constexpr Status PROXY_AUTHENTICATION_REQUIRED = 407;
	inline constexpr Status REQUEST_TIMEOUT = 408;
	inline constexpr Status CONFLICT = 409;
	inline constexpr Status GONE = 410;
	inline constexpr Status LENGTH_REQUIRED = 411;
	inline constexpr Status PRECONDITION_FAILED = 412;
	inline constexpr Status PAYLOAD_TOO_LARGE = 413;
	inline constexpr Status URI_TOO_LONG = 414;
	inline constexpr Status UNSUPPORTED_MEDIA_TYPE = 415;
	inline constexpr Status RANGE_NOT_SATISFIABLE = 416;
	inline constexpr Status EXPECTATION_FAILED = 417;
	inline constexpr Status I_AM_A_TEAPOT = 418;
	inline constexpr Status MISDIRECTED_REQUEST = 421;
	inline constexpr Status UNPROCESSABLE_ENTITY = 422;
	inline constexpr Status LOCKED = 423;
	inline constexpr Status FAILED_DEPENDENCY = 424;
	inline constexpr Status TOO_EARLY = 425;
	inline constexpr Status UPGRADE_REQUIRED = 426;
	inline constexpr Status PRECONDITION_REQUIRED = 428;
	inline constexpr Status TOO_MANY_REQUESTS = 429;
	inline constexpr Status REQUEST_HEADER_FIELDS_TOO_LARGE = 431;
	inline constexpr Status UNAVAILABLE_FOR_LEGAL_REASONS = 451;
	
	inline constexpr Status INTERNAL_SERVER_ERROR = 500;
	inline constexpr Status NOT_IMPLEMENTED = 501;
	inline constexpr Status BAD_GATEWAY = 502;
	inline constexpr Status SERVICE_UNAVAILABLE = 503;
	inline constexpr Status GATEWAY_TIMEOUT = 504;
	inline constexpr Status HTTP_VERSION_NOT_SUPPORTED = 505;
	inline constexpr Status VARIANT_ALSO_NEGOTIATES = 506;
	inline constexpr Status INSUFFICIENT_STORAGE = 507;
	inline constexpr Status LOOP_DETECTED = 508;
	inline constexpr Status NOT_EXTENDED = 510;
	inline constexpr Status NETWORK_AUTHENTICATION_REQUIRED = 511;
} // namespace status

class Request {
	friend Server;
	std::shared_ptr<ConnectionState> m;
public:
	
	class Path {
		friend Server;
		friend Request;
		std::shared_ptr<ConnectionState> m;
	public:
		
		std::unordered_map<std::string, std::string> &map();
		const std::unordered_map<std::string, std::string> &map() const;
		
		const std::string &string() const;
		const std::string &string_raw() const;
		
		std::string &
		operator[](const std::string &key)
		{ return map().at(key); }
		
		const std::string &
		operator[](const std::string &key) const
		{ return map().at(key); }
		
		template<class T, class Converter = StringConverter<T>>
		std::optional<T>
		get(const std::string &key) const
		{
			T val;
			if (Converter::convert(val, this->operator[](key))) {
				return {val};
			} else {
				return {};
			}
		}
		
		template<class T, class Converter = StringConverter<T>>
		T
		get_or(const std::string &key, T default_value = T()) const
		{
			T val;
			if (Converter::convert(val, this->operator[](key))) {
				return val;
			} else {
				return default_value;
			}
		}
		
	}; // class Request::Path
	
	class Query {
		friend Server;
		friend Request;
		std::shared_ptr<ConnectionState> m;
	public:
		
		struct IterateAllResult {
			std::pair<
				std::unordered_map<std::string, std::string>::iterator,
				std::unordered_map<std::string, std::string>::iterator
			> equal_range;
			auto begin() const noexcept { return equal_range.first; }
			auto end() const noexcept { return equal_range.second; }
		}; // class IterateAllResult
		
		struct IterateAllResultConst {
			std::pair<
				std::unordered_map<std::string, std::string>::const_iterator,
				std::unordered_map<std::string, std::string>::const_iterator
			> equal_range;
			auto begin() const noexcept { return equal_range.first; }
			auto end() const noexcept { return equal_range.second; }
		}; // class IterateAllResultConst
		
		std::unordered_multimap<std::string, std::string> &map();
		const std::unordered_multimap<std::string, std::string> &map() const;
		
		const std::string &string() const;
		const std::string &string_raw() const;
		
		std::string &
		operator[](const std::string &key)
		{ return map().find(key)->second; }
		
		const std::string &
		operator[](const std::string &key) const
		{ return map().find(key)->second; }
		
		auto
		get_all(const std::string &key)
		{ return map().equal_range(key); }
		
		auto
		get_all(const std::string &key) const
		{ return map().equal_range(key); }
		
		IterateAllResult
		iterate_all(const std::string &key)
		{ return {map().equal_range(key)}; }
		
		IterateAllResultConst
		iterate_all(const std::string &key) const
		{ return {map().equal_range(key)}; }
		
		template<class T, class Converter = StringConverter<T>>
		std::optional<T>
		get(const std::string &key) const
		{
			T val;
			if (map().contains(key) && Converter::convert(val, this->operator[](key))) {
				return {val};
			} else {
				return {};
			}
		}
		
		template<class T, class Converter = StringConverter<T>>
		T
		get_or(const std::string &key, T default_value = T()) const
		{
			T val;
			if (map().contains(key) && Converter::convert(val, this->operator[](key))) {
				return val;
			} else {
				return default_value;
			}
		}
		
	}; // class Request::Query
	
	class Body {
		friend Server;
		friend Request;
		std::shared_ptr<ConnectionState> m;
	public:
		
		std::iostream &stream();
		const std::iostream &stream() const;
		
		template<class T>
		std::istream &
		operator>>(T &&arg)
		{ return stream() >> std::forward<T>(arg); }
		
		template<class T>
		std::ostream &
		operator<<(T &&arg)
		{ return stream() << std::forward<T>(arg); }
		
	}; // class Request::Body
	
	template<class Middleware>
	Middleware &
	middleware() const
	{
		static_assert(
			std::is_base_of<MiddlewareI, Middleware>::value,
			"The middleware class must inherit from woof::MiddlewareI"
		);
		static_assert(
			std::is_default_constructible<Middleware>::value,
			"The middleware class must be default-constructible"
		);
		size_t type_hash = typeid(Middleware).hash_code();
		return static_cast<Middleware &>(do_middleware(type_hash));
	}
	
	Method method() const;
	
	const std::string &target_string() const;
	const std::string &target_string_raw() const;
	
	Path &path() { return m_path; }
	const Path &path() const { return m_path; }
	
	Query &query() { return m_query; }
	const Query &query() const { return m_query; }
	
	HeaderMap &headers();
	const HeaderMap &headers() const;
	
	Body &body() { return m_body; }
	const Body &body() const { return m_body; }
	
	Request(const Request &) noexcept = default;
	Request(std::shared_ptr<ConnectionState> m_ = {}) noexcept
	{
		m = m_;
		m_path.m = m_;
		m_query.m = m_;
		m_body.m = m_;
	}
	
private:
	
	MiddlewareI &do_middleware(size_t hash) const;
	
	Path m_path;
	Query m_query;
	Body m_body;
}; // class Request

class Response {
	friend Server;
	std::shared_ptr<ConnectionState> m;
public:
	
	class Body {
		friend Server;
		friend Response;
		std::shared_ptr<ConnectionState> m;
	public:
		
		std::iostream &stream();
		const std::iostream &stream() const;
		
		template<class T>
		std::istream &
		operator>>(T &&arg)
		{ return stream() >> std::forward<T>(arg); }
		
		template<class T>
		std::ostream &
		operator<<(T &&arg)
		{ return stream() << std::forward<T>(arg); }
		
	}; // class Response::Body
	
	Status status() const;
	void status(Status status);
	
	HeaderMap &headers();
	const HeaderMap &headers() const;
	
	Body &body() { return m_body; }
	const Body &body() const { return m_body; }
	
	Response(const Response &) noexcept = default;
	Response(std::shared_ptr<ConnectionState> m_ = {}) noexcept
	{
		m = m_;
		m_body.m = m_;
	}
	
private:
	Body m_body;
}; // class Response

class Server {
	std::shared_ptr<ServerState> m;
public:
	
	Server();
	
	Server &logger(const LogHandler &handler); //! The end user has to make sure it's thread safe
	Server &address(const std::string &address);
	Server &port(int port);
	
	template<class Middleware>
	void
	add_middleware(const PathPattern &path)
	{
		static_assert(
			std::is_base_of<MiddlewareI, Middleware>::value,
			"The middleware class must inherit from woof::MiddlewareI"
		);
		static_assert(
			std::is_default_constructible<Middleware>::value,
			"The middleware class must be default-constructible"
		);
		size_t type_hash = typeid(Middleware).hash_code();
		do_add_middleware(type_hash, path, [] { return new Middleware(); });
	}
	
	template<class Middleware>
	void
	add_middleware(const std::string &pattern)
	{ add_middleware<Middleware>(PathPattern::make(pattern)); }
	
	template<class Middleware, StringConstant pattern>
	void
	add_middleware()
	{ add_middleware<Middleware>(PathPattern::make<pattern>()); }
	
	void run(int nworkers);
	
	void add_endpoint(Method method, const PathPattern &path, const RequestHandler &handler);
	
	void
	add_endpoint(Method method, const std::string &path, const RequestHandler &handler)
	{ add_endpoint(method, PathPattern::make(path), handler); }
	
	template<StringConstant path>
	void
	add_endpoint(Method method, const RequestHandler &handler)
	{ add_endpoint(method, PathPattern::make<path>(), handler); }
	
	void
	GET(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::GET, path, handler); }
	
	template<StringConstant path>
	void
	GET(const RequestHandler &handler)
	{ add_endpoint<path>(Method::GET, handler); }
	
	void
	HEAD(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::HEAD, path, handler); }
	
	template<StringConstant path>
	void
	HEAD(const RequestHandler &handler)
	{ add_endpoint<path>(Method::HEAD, handler); }
	
	void
	POST(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::POST, path, handler); }
	
	template<StringConstant path>
	void
	POST(const RequestHandler &handler)
	{ add_endpoint<path>(Method::POST, handler); }
	
	void
	PUT(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::PUT, path, handler); }
	
	template<StringConstant path>
	void
	PUT(const RequestHandler &handler)
	{ add_endpoint<path>(Method::PUT, handler); }
	
	void
	DELETE(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::DELETE, path, handler); }
	
	template<StringConstant path>
	void
	DELETE(const RequestHandler &handler)
	{ add_endpoint<path>(Method::DELETE, handler); }
	
	void
	CONNECT(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::CONNECT, path, handler); }
	
	template<StringConstant path>
	void
	CONNECT(const RequestHandler &handler)
	{ add_endpoint<path>(Method::CONNECT, handler); }
	
	void
	OPTIONS(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::OPTIONS, path, handler); }
	
	template<StringConstant path>
	void
	OPTIONS(const RequestHandler &handler)
	{ add_endpoint<path>(Method::OPTIONS, handler); }
	
	void
	TRACE(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::TRACE, path, handler); }
	
	template<StringConstant path>
	void
	TRACE(const RequestHandler &handler)
	{ add_endpoint<path>(Method::TRACE, handler); }
	
	void
	PATCH(const auto &path, const RequestHandler &handler)
	{ add_endpoint(Method::PATCH, path, handler); }
	
	template<StringConstant path>
	void
	PATCH(const RequestHandler &handler)
	{ add_endpoint<path>(Method::PATCH, handler); }
	
private:
	
	void do_add_middleware(size_t hash, const PathPattern &path, const MiddlewareCreator &mw);
}; // class Server

////////////////////////////////////////////////////////////////////////////////////////////////////

// signed integer types

template<>
struct StringConverter<short> {
	static bool convert(short &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<int> {
	static bool convert(int &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<long> {
	static bool convert(long &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<long long> {
	static bool convert(long long &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

// unsigned integer types

template<>
struct StringConverter<unsigned short> {
	static bool convert(unsigned short &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<unsigned int> {
	static bool convert(unsigned int &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<unsigned long> {
	static bool convert(unsigned long &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<unsigned long long> {
	static bool convert(unsigned long long &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

// floating point types

template<>
struct StringConverter<float> {
	static bool convert(float &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<double> {
	static bool convert(double &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

template<>
struct StringConverter<long double> {
	static bool convert(long double &x, const std::string &s) {
		return std::from_chars(&s.front(), &s.back() + 1, x).ec == std::errc();
	}
};

// bool

template<>
struct StringConverter<bool> {
	static bool convert(bool &x, const std::string &s) {
		if (s.empty())   goto yes;
		if (s == "1")    goto yes;
		if (s == "true") goto yes;
		if (s == "on")   goto yes;
		if (s == "yes")  goto yes;
		
		if (s == "0")     goto no;
		if (s == "false") goto no;
		if (s == "off")   goto no;
		if (s == "no")    goto no;
		
		return false;
		yes: x = true; return true;
		no: x = false; return true;
	}
};

} // namespace woof

#endif // C++ 20 check

#endif
