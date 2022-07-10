#include "internal.hpp"

namespace woof {

// A state machine parser for path patterns. See <woof/path_pattern_sfinae.hpp> for more info.

PathPattern
PathPattern::make(const std::string &pattern)
{
	const size_t sz = pattern.size();
	enum class State { SLASH, IN_LITERAL, LBRACE, IN_NAME, RBRACE, STAR1, STAR2 } state = State::SLASH;
	std::string buf;
	
	PathPattern pp;
	
	for (size_t idx = pattern[0] == '/'; idx < sz; ++idx) {
		const char c = pattern[idx];
		
		switch (state) {
		case State::SLASH: {
			if (c == '\0') {
			} else if (c == '{') {
				state = State::LBRACE;
			} else if (c == '*') {
				state = State::STAR1;
			} else if (c == '/') {
				pp.segments.emplace_back();
				state = State::SLASH;
			} else {
				buf.push_back(c);
				state = State::IN_LITERAL;
			}
		} break;
		case State::IN_LITERAL: {
			if (c == '\0' || c == '/') {
				pp.segments.push_back({false, std::move(buf)});
				state = State::SLASH;
			} else {
				buf.push_back(c);
			}
		} break;
		case State::LBRACE: {
			if (c == '\0') {
				// TODO error
			} else if (c == '}') {
				pp.segments.push_back({true});
				state = State::RBRACE;
			} else {
				buf.push_back(c);
			}
		} break;
		case State::IN_NAME: {
			if (c == '\0') {
				// TODO error
			} else if (c == '}') {
				pp.segments.push_back({true, std::move(buf)});
				state = State::RBRACE;
			} else {
				buf.push_back(c);
			}
		} break;
		case State::RBRACE: {
			if (c == '\0' || c == '/') {
				state = State::SLASH;
			} else {
				// TODO error
			}
		} break;
		case State::STAR1: {
			if (c == '\0' || c == '/') {
				pp.segments.push_back({true});
				state = State::SLASH;
			} else if (c == '*') {
				state = State::STAR2;
			} else {
				// TODO error
			}
		} break;
		case State::STAR2: {
			if (c == '\0') {
				pp.suffix_wildcard = true;
			} else {
				// TODO error
			}
		} break;
		}
	}
	
	return pp;
}

}
