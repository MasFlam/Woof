#include <woof/woof.hpp>
#include <cctype>

namespace woof {

// Hash with 64bit FNV1a
size_t CaseInsensitiveHash::operator()(const std::string &s) const {
	uint64_t hash = 0xcbf29ce484222325;
	const uint64_t prime = 0x100000001b3;
	for (char c : s) {
		hash ^= std::toupper(c);
		hash *= prime;
	}
	return hash;
}

bool CaseInsensitiveEquals::operator()(const std::string &a, const std::string &b) const {
	size_t len = a.size();
	if (len != b.size()) return false;
	for (size_t i = 0; i < len; ++i) {
		if (std::toupper(a[i]) != std::toupper(b[i])) return false;
	}
	return true;
}

}
