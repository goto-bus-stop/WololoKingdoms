#include "string_helpers.h"
#include <algorithm>
#include <cctype> // std::tolower
#include <iconv.h>
#include <istream>
#include <memory>
#include <string>

void replace_all(std::string& str, const std::string& from,
                 const std::string& to) {
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}

std::wstring strtowstr(const std::string& narrow) {
  std::wstring wide;
  for (auto c : narrow) {
    wchar_t w = c;
    wide.push_back(w);
  }
  return wide;
}

std::string wstrtostr(const std::wstring& wide) {
  std::string narrow;
  for (auto c : wide) {
    char n = c;
    narrow.push_back(n);
  }
  return narrow;
}

std::string tolower(std::string line) {
  std::transform(line.begin(), line.end(), line.begin(),
                 static_cast<int (*)(int)>(std::tolower));
  return line;
}

std::string concat_stream(std::istream& stream) {
  std::stringstream strstr;
  strstr << stream.rdbuf();
  return strstr.str();
}

std::string iconvert(
    std::string& input,
    const std::string& from,
    const std::string& to) {
#ifdef ICONV_SECOND_ARGUMENT_IS_CONST
  auto in_str = input.c_str();
#else
  auto in_str = &input[0];
#endif
  auto in_size = input.length();
  size_t out_size = in_size * 2;
  auto result = std::make_unique<char[]>(out_size);
  char* out = result.get(); // separate value because iconv advances the pointer

  iconv_t convert = iconv_open(to.c_str(), from.c_str());
  if (convert == (iconv_t)-1) {
    return "";
  }
  if (iconv(convert, &in_str, &in_size, &out, &out_size) == (size_t)-1) {
    return "";
  }
  *out = '\0';

  iconv_close(convert);

  return result.get();
}
