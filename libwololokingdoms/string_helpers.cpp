#include <string>
#include <istream>
#include <algorithm>
#include "string_helpers.h"

void replace_all(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::wstring strtowstr(std::string narrow) {
    std::wstring wide;
    for (auto c : narrow) {
        wchar_t w = c;
        wide.push_back(w);
    }
    return wide;
}

std::string wstrtostr(std::wstring wide) {
    std::string narrow;
    for (auto c : wide) {
        char n = c;
        narrow.push_back(n);
    }
    return narrow;
}

std::string tolower(std::string line) {
	std::transform(line.begin(), line.end(), line.begin(), static_cast<int(*)(int)>(std::tolower));
	return line;
}

std::string concat_stream(std::istream& stream) {
    std::stringstream strstr;
    strstr << stream.rdbuf();
    return strstr.str();
}