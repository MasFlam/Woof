#include "internal.hpp"

namespace woof {

/*ParsedTarget::ParsedTarget(const std::string_view &sv)
{
	enum {
		STATE_PATH,
		STATE_QUERY_NAME,
		STATE_QUERY_VALUE
	} state = STATE_PATH;
	size_t name_start, name_end;
	size_t segment_start = sv[0] == '/';
	
	for (size_t i = segment_start; i <= sv.size(); ++i) {
		bool last = i == sv.size();
		char c = last ? '\0' : sv[i];
		switch (state) {
		case STATE_PATH:
			if (c == '/' || ((c == '?' || last) && segment_start != i)) {
				path_segments.push_back(sv.substr(segment_start, i - segment_start));
				segment_start = i+1;
			}
			if (c == '?') state = STATE_QUERY_NAME;
			if (c == '?' || last) {
				name_start = i+1;
				path = sv.substr(0, i);
			}
			break;
		case STATE_QUERY_NAME:
			if (c == '=') {
				name_end = i;
				state = STATE_QUERY_VALUE;
			}
			break;
		case STATE_QUERY_VALUE:
			if (c == '&' || last) {
				size_t value_start = name_end+1;
				query_params.emplace_back(
					sv.substr(name_start, name_end - name_start),
					sv.substr(value_start, i - value_start)
				);
				name_start = i+1;
				state = STATE_QUERY_NAME;
			}
		}
	}
	
	if (state > STATE_PATH) {
		query = sv.substr(path.size() + 1);
	}
}*/

static constexpr unsigned char
quartet_from_hex_char(char c) noexcept
{
	if (c >= '0' || c <= '9') return c - '0';
	if (c >= 'A' || c <= 'F') return c - 'A' + 10;
	if (c >= 'a' || c <= 'f') return c - 'a' + 10;
	return 16;
}

static constexpr int
decode_percent(const char percent[2]) noexcept
{
	unsigned char high = quartet_from_hex_char(percent[0]);
	unsigned char low  = quartet_from_hex_char(percent[1]);
	if (high > 15 || low > 15) return std::numeric_limits<char>::max() + 1;
	unsigned char c = (high << 4) & low;
	return *reinterpret_cast<char *>(&c);
}

ParsedTarget::ParsedTarget(const std::string_view &sv)
{
	const size_t sz = sv.size();
	enum { STATE_PATH, STATE_QUERY_NAME, STATE_QUERY_VALUE } state = STATE_PATH;
	enum { PERCENT_NONE, PERCENT_PERCENT, PERCENT_END } percent_state = PERCENT_NONE;
	char percent_chars[2];
	std::string buf, buf2;
	
	char first_char = sv[0];
	bool first_is_slash = first_char == '/';
	
	if (first_is_slash) path.push_back(first_char);
	
	for (size_t idx = first_is_slash; idx <= sz; ++idx) {
		bool last = idx == sz;
		char c = last ? '\0' : sv[idx];
		
		switch (percent_state) {
		case PERCENT_NONE: break;
		case PERCENT_PERCENT: {
			percent_chars[0] = c;
			percent_state = PERCENT_END;
			continue;
		} break;
		case PERCENT_END: {
			percent_chars[1] = c;
			int ch = decode_percent(percent_chars);
			if (ch > std::numeric_limits<char>::max()) {
				success = false;
				return;
			}
			buf.push_back(ch);
			percent_state = PERCENT_NONE;
		} break;
		}
		
		if (c == '%') {
			percent_state = PERCENT_PERCENT;
			continue;
		}
		
		if (c != '\0') decoded.push_back(c);
		
		switch (state) {
		case STATE_PATH: {
			if (c == '?' || c == '\0') {
				path_raw = sv.substr(0, idx);
			} else {
				path.push_back(c);
			}
			if (c == '/' || c == '?' || c == '\0') {
				path_segments.emplace_back(std::move(buf));
				if (c == '?') {
					state = STATE_QUERY_NAME;
				}
			} else {
				buf.push_back(c);
			}
		} break;
		case STATE_QUERY_NAME: {
			if (c == '+') c = ' ';
			if (c != '\0') query.push_back(c);
			if (c == '=') {
				state = STATE_QUERY_VALUE;
			} else if (c == '&' || c == '\0') {
				query_params.emplace_back(std::move(buf), std::string());
			} else {
				buf.push_back(c);
			}
		} break;
		case STATE_QUERY_VALUE: {
			if (c == '+') c = ' ';
			if (c != '\0') query.push_back(c);
			if (c == '&' || c == '\0') {
				query_params.emplace_back(std::move(buf), std::move(buf2));
				state = STATE_QUERY_NAME;
			} else {
				buf2.push_back(c);
			}
		} break;
		}
	}
	
	if (percent_state == PERCENT_NONE && buf.empty() && buf2.empty()) {
		success = true;
		query_raw = sv.substr(path_raw.size() + (state != STATE_PATH));
	} else {
		success = false;
	}
}

}
