
/*
	State machine parser in SFINAE for path patterns, with (hopefully) helpful error messages.
	This file is included inside namespace woof in <woof/woof.hpp>
	
	Worst case compile-time complexity is O(n^2). (unless the compiler is smart about variadic
	templates, then O(n) I guess) But that's when the entire string is a named param.
	
	How it works:

push(bool wildcard, string name) -- push a segment
append -- append the current character to the buffer
dumpbuf(bool wildcard) -- push a segment with the name being the buffer, and clear the buffer

	STATE_SLASH:
		'\0' =>
		'{' => state LBRACE, next
		'*' => state STAR1, next
		'/' => push(false, ""), state SLASH, next
		else => append, state IN_LITERAL, next

	STATE_IN_LITERAL:
		'\0' => dumpbuf(false)
		'/' => dumpbuf(false), state STATE_SLASH, next
		else => append, next

	STATE_LBRACE:
		'\0' => error
		'}' => push(true, ""), state STATE_RBRACE, next
		else => append, next

	STATE_IN_NAME:
		'\0' => error
		'}' => dumpbuf(true), state STATE_RBRACE, next
		else => append, next

	STATE_RBRACE:
		'\0' =>
		'/' => state STATE_SLASH, next
		else => error

	STATE_STAR1:
		'\0' => push(true, "")
		'/' => push(true, ""), state STATE_SLASH, next
		'*' => state STATE_STAR2, next
		else => error

	STATE_STAR2:
		'\0' => suffix_wildcard=true
		else => error
*/

namespace {

enum class State { SLASH, IN_LITERAL, LBRACE, IN_NAME, RBRACE, STAR1, STAR2 };

template<size_t N>
struct Data {
	StringConstant<N> sc;
	size_t idx;
	
	constexpr Data
	next() const noexcept
	{ return {sc, idx+1}; }
	
	constexpr char
	operator[](size_t idx) const noexcept
	{ return sc[idx]; }
};

template<auto...>
static constexpr bool False = false;

template<Data data, State state, char ch, char... buf>
struct Do;

template<Data data, State state, char... buf>
using Next = Do<Data<data.sc.length+1>{data.sc, data.idx+1}, state, data.sc[data.idx+1], buf...>;

template<Data data, State state, char... buf>
using Append = Do<Data<data.sc.length+1>{data.sc, data.idx+1}, state, data.sc[data.idx+1], buf..., data.sc[data.idx]>;

template<bool wildcard, char... buf>
inline constexpr void
Push(auto &pp) noexcept
{
	pp.segments.push_back({wildcard, {buf...}});
}

// state SLASH

template<Data data>
struct Do<data, State::SLASH, '\0'> {
	static constexpr void path(auto &pp) noexcept {}
};

template<Data data>
struct Do<data, State::SLASH, '{'> {
	static constexpr void path(auto &pp) noexcept {
		Next<data, State::LBRACE>::path(pp);
	}
};

template<Data data>
struct Do<data, State::SLASH, '*'> {
	static constexpr void path(auto &pp) noexcept {
		Next<data, State::STAR1>::path(pp);
	}
};

template<Data data>
struct Do<data, State::SLASH, '/'> {
	static constexpr void path(auto &pp) noexcept {
		Push<false>(pp);
		Next<data, State::SLASH>::path(pp);
	}
};

template<Data data, char ch>
struct Do<data, State::SLASH, ch> {
	static constexpr void path(auto &pp) noexcept {
		Append<data, State::IN_LITERAL>::path(pp);
	}
};

// state IN_LITERAL

template<Data data, char... buf>
struct Do<data, State::IN_LITERAL, '\0', buf...> {
	static constexpr void path(auto &pp) noexcept {
		Push<false, buf...>(pp);
	}
};

template<Data data, char... buf>
struct Do<data, State::IN_LITERAL, '/', buf...> {
	static constexpr void path(auto &pp) noexcept {
		Push<false, buf...>(pp);
		Next<data, State::SLASH>::path(pp);
	}
};

template<Data data, char ch, char... buf>
struct Do<data, State::IN_LITERAL, ch, buf...> {
	static constexpr void path(auto &pp) noexcept {
		Append<data, State::IN_LITERAL, buf...>::path(pp);
	}
};

// state LBRACE

template<Data data>
struct Do<data, State::LBRACE, '\0'> {
	static constexpr void path(auto &pp) noexcept {
		static_assert(False<data.idx>, "Unclosed named path param");
	}
};

template<Data data>
struct Do<data, State::LBRACE, '}'> {
	static constexpr void path(auto &pp) noexcept {
		Push<true>(pp);
		Next<data, State::RBRACE>::path(pp);
	}
};

template<Data data, char ch>
struct Do<data, State::LBRACE, ch> {
	static constexpr void path(auto &pp) noexcept {
		Append<data, State::IN_NAME>::path(pp);
	}
};

// state IN_NAME

template<Data data, char... buf>
struct Do<data, State::IN_NAME, '\0', buf...> {
	static constexpr void path(auto &pp) noexcept {
		static_assert(False<data.idx>, "Unclosed named path param");
	}
};

template<Data data, char... buf>
struct Do<data, State::IN_NAME, '}', buf...> {
	static constexpr void path(auto &pp) noexcept {
		Push<true, buf...>(pp);
		Next<data, State::RBRACE>::path(pp);
	}
};

template<Data data, char ch, char... buf>
struct Do<data, State::IN_NAME, ch, buf...> {
	static constexpr void path(auto &pp) noexcept {
		Append<data, State::IN_NAME, buf...>::path(pp);
	}
};

// state RBRACE

template<Data data>
struct Do<data, State::RBRACE, '\0'> {
	static constexpr void path(auto &pp) noexcept {}
};

template<Data data>
struct Do<data, State::RBRACE, '/'> {
	static constexpr void path(auto &pp) noexcept {
		Next<data, State::SLASH>::path(pp);
	}
};

template<Data data, char ch>
struct Do<data, State::RBRACE, ch> {
	static constexpr void path(auto &pp) noexcept {
		static_assert(False<data.idx>, "Garbage after closing brace");
	}
};

// state STAR1

template<Data data>
struct Do<data, State::STAR1, '\0'> {
	static constexpr void path(auto &pp) noexcept {
		Push<true>(pp);
	}
};

template<Data data>
struct Do<data, State::STAR1, '/'> {
	static constexpr void path(auto &pp) noexcept {
		Push<true>(pp);
		Next<data, State::SLASH>::path(pp);
	}
};

template<Data data>
struct Do<data, State::STAR1, '*'> {
	static constexpr void path(auto &pp) noexcept {
		Next<data, State::STAR2>::path(pp);
	}
};

template<Data data, char ch>
struct Do<data, State::STAR1, ch> {
	static constexpr void path(auto &pp) noexcept {
		static_assert(False<data.idx>, "Garbage after star");
	}
};

// state STAR2

template<Data data>
struct Do<data, State::STAR2, '\0'> {
	static constexpr void path(auto &pp) noexcept {
		pp.suffix_wildcard = true;
	}
};

template<Data data, char ch>
struct Do<data, State::STAR2, ch> {
	static constexpr void path(auto &pp) noexcept {
		static_assert(False<data.idx>, "Double star can only appear at the end of the pattern");
	}
};

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

template<StringConstant sc, size_t idx>
struct _path {
	static constexpr void path(auto &pp) {
		//_do_path<sc, idx, STATE_SLASH, idx, sc.chars[idx]>::path(pp);
		Do<Data<sc.length+1>{sc, idx}, State::SLASH, sc[idx]>::path(pp);
	}
};
