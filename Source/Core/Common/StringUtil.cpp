// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <istream>
#include <iterator>
#include <limits.h>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#ifdef _WIN32
#include <locale>
#include <codecvt>
#endif

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
#include <Windows.h>
constexpr u32 CODEPAGE_SHIFT_JIS = 932;
constexpr u32 CODEPAGE_WINDOWS_1252 = 1252;
#else
#include <errno.h>
#include <iconv.h>
#include <locale.h>
#endif

#if !defined(_WIN32) && !defined(ANDROID) && !defined(__OpenBSD__)
static locale_t GetCLocale()
{
	static locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
	return c_locale;
}
#endif

std::string HexDump(const u8* data, size_t size)
{
	constexpr size_t BYTES_PER_LINE = 16;

	std::string out;
	for (size_t row_start = 0; row_start < size; row_start += BYTES_PER_LINE)
	{
		out += StringFromFormat("%06zx: ", row_start);
		for (size_t i = 0; i < BYTES_PER_LINE; ++i)
		{
			if (row_start + i < size)
			{
				out += StringFromFormat("%02hhx ", data[row_start + i]);
			}
			else
			{
				out += "   ";
			}
		}
		out += " ";
		for (size_t i = 0; i < BYTES_PER_LINE; ++i)
		{
			if (row_start + i < size)
			{
				char c = static_cast<char>(data[row_start + i]);
				out += StringFromFormat("%c", isprint(c) ? c : '.');
			}
		}
		out += "\n";
	}
	return out;
}

// faster than sscanf
bool AsciiToHex(const std::string& _szValue, u32& result)
{
	// Set errno to a good state.
	errno = 0;

	char* endptr = nullptr;
	const u32 value = strtoul(_szValue.c_str(), &endptr, 16);

	if (!endptr || *endptr)
		return false;

	if (errno == ERANGE)
		return false;

	result = value;
	return true;
}

bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args)
{
	int writtenCount;

#ifdef _WIN32
	// You would think *printf are simple, right? Iterate on each character,
	// if it's a format specifier handle it properly, etc.
	//
	// Nooooo. Not according to the C standard.
	//
	// According to the C99 standard (7.19.6.1 "The fprintf function")
	//     The format shall be a multibyte character sequence
	//
	// Because some character encodings might have '%' signs in the middle of
	// a multibyte sequence (SJIS for example only specifies that the first
	// byte of a 2 byte sequence is "high", the second byte can be anything),
	// printf functions have to decode the multibyte sequences and try their
	// best to not screw up.
	//
	// Unfortunately, on Windows, the locale for most languages is not UTF-8
	// as we would need. Notably, for zh_TW, Windows chooses EUC-CN as the
	// locale, and completely fails when trying to decode UTF-8 as EUC-CN.
	//
	// On the other hand, the fix is simple: because we use UTF-8, no such
	// multibyte handling is required as we can simply assume that no '%' char
	// will be present in the middle of a multibyte sequence.
	//
	// This is why we look up the default C locale here and use _vsnprintf_l.
	static _locale_t c_locale = nullptr;
	if (!c_locale)
		c_locale = _create_locale(LC_ALL, "C");
	writtenCount = _vsnprintf_l(out, outsize, format, c_locale, args);
#else
#if !defined(ANDROID) && !defined(__OpenBSD__)
	locale_t previousLocale = uselocale(GetCLocale());
#endif
	writtenCount = vsnprintf(out, outsize, format, args);
#if !defined(ANDROID) && !defined(__OpenBSD__)
	uselocale(previousLocale);
#endif
#endif

	if (writtenCount > 0 && writtenCount < outsize)
	{
		out[writtenCount] = '\0';
		return true;
	}
	else
	{
		out[outsize - 1] = '\0';
		return false;
	}
}

std::string StringFromFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	std::string res = StringFromFormatV(format, args);
	va_end(args);
	return res;
}

std::string StringFromFormatV(const char* format, va_list args)
{
	char* buf = nullptr;
#ifdef _WIN32
	int required = _vscprintf(format, args);
	buf = new char[required + 1];
	CharArrayFromFormatV(buf, required + 1, format, args);

	std::string temp = buf;
	delete[] buf;
#else
#if !defined(ANDROID) && !defined(__OpenBSD__)
	locale_t previousLocale = uselocale(GetCLocale());
#endif
	if (vasprintf(&buf, format, args) < 0)
		ERROR_LOG(COMMON, "Unable to allocate memory for string");
#if !defined(ANDROID) && !defined(__OpenBSD__)
	uselocale(previousLocale);
#endif

	std::string temp = buf;
	free(buf);
#endif
	return temp;
}

// For Debugging. Read out an u8 array.
std::string ArrayToString(const u8* data, u32 size, int line_len, bool spaces)
{
	std::ostringstream oss;
	oss << std::setfill('0') << std::hex;

	for (int line = 0; size; ++data, --size)
	{
		oss << std::setw(2) << (int)*data;

		if (line_len == ++line)
		{
			oss << '\n';
			line = 0;
		}
		else if (spaces)
			oss << ' ';
	}

	return oss.str();
}

// Turns "  hej " into "hej". Also handles tabs.
std::string StripSpaces(const std::string& str)
{
	const size_t s = str.find_first_not_of(" \t\r\n");

	if (str.npos != s)
		return str.substr(s, str.find_last_not_of(" \t\r\n") - s + 1);
	else
		return "";
}

// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripQuotes(const std::string& s)
{
	if (s.size() && '\"' == s[0] && '\"' == *s.rbegin())
		return s.substr(1, s.size() - 2);
	else
		return s;
}

bool TryParse(const std::string& str, u32* const output)
{
	char* endptr = nullptr;

	// Reset errno to a value other than ERANGE
	errno = 0;

	unsigned long value = strtoul(str.c_str(), &endptr, 0);

	if (!endptr || *endptr)
		return false;

	if (errno == ERANGE)
		return false;

#if ULONG_MAX > UINT_MAX
	if (value >= 0x100000000ull && value <= 0xFFFFFFFF00000000ull)
		return false;
#endif

	*output = static_cast<u32>(value);
	return true;
}

bool TryParse(const std::string& str, bool* const output)
{
	float value;
	const bool is_valid_float = TryParse(str, &value);
	if ((is_valid_float && value == 1) || !strcasecmp("true", str.c_str()))
		*output = true;
	else if ((is_valid_float && value == 0) || !strcasecmp("false", str.c_str()))
		*output = false;
	else
		return false;

	return true;
}

std::string StringFromInt(int value)
{
	char temp[16];
	sprintf(temp, "%i", value);
	return temp;
}

std::string StringFromBool(bool value)
{
	return value ? "True" : "False";
}

bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename,
	std::string* _pExtension)
{
	if (full_path.empty())
		return false;

	size_t dir_end = full_path.find_last_of("/"
		// Windows needs the : included for something like just "C:" to be considered a directory
#ifdef _WIN32
		":"
#endif
	);
	if (std::string::npos == dir_end)
		dir_end = 0;
	else
		dir_end += 1;

	size_t fname_end = full_path.rfind('.');
	if (fname_end < dir_end || std::string::npos == fname_end)
		fname_end = full_path.size();

	if (_pPath)
		*_pPath = full_path.substr(0, dir_end);

	if (_pFilename)
		*_pFilename = full_path.substr(dir_end, fname_end - dir_end);

	if (_pExtension)
		*_pExtension = full_path.substr(fname_end);

	return true;
}

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path,
	const std::string& _Filename)
{
	_CompleteFilename = _Path;

	// check for seperator
	if (DIR_SEP_CHR != *_CompleteFilename.rbegin())
		_CompleteFilename += DIR_SEP_CHR;

	// add the filename
	_CompleteFilename += _Filename;
}

void SplitString(const std::string& str, const char delim, std::vector<std::string>& output)
{
	std::istringstream iss(str);
	output.resize(1);

	while (std::getline(iss, *output.rbegin(), delim))
		output.push_back("");

	output.pop_back();
}

std::string JoinStrings(const std::vector<std::string>& strings, const std::string& delimiter)
{
	// Check if we can return early, just for speed
	if (strings.empty())
		return "";

	std::stringstream res;
	std::copy(strings.begin(), strings.end(),
		std::ostream_iterator<std::string>(res, delimiter.c_str()));

	// Drop the trailing delimiter.
	std::string joined = res.str();
	return joined.substr(0, joined.length() - delimiter.length());
}

std::string TabsToSpaces(int tab_size, const std::string& in)
{
	const std::string spaces(tab_size, ' ');
	std::string out(in);

	size_t i = 0;
	while (out.npos != (i = out.find('\t')))
		out.replace(i, 1, spaces);

	return out;
}

std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest)
{
	size_t pos = 0;

	if (src == dest)
		return result;

	while ((pos = result.find(src, pos)) != std::string::npos)
	{
		result.replace(pos, src.size(), dest);
		pos += dest.length();
	}

	return result;
}

void ConvertNarrowSpecialSHIFTJIS(std::string &input)
{
	// Melee doesn't correctly display special characters in narrow form We need to convert them to wide form.
	// I couldn't find a library to do this so for now let's just do it manually
	static std::unordered_map<char, char16_t> specialCharConvert = {
	    {'!', (char16_t)0x8149}, {'"', (char16_t)0x8168}, {'#', (char16_t)0x8194},  {'$', (char16_t)0x8190},
	    {'%', (char16_t)0x8193}, {'&', (char16_t)0x8195}, {'\'', (char16_t)0x8166}, {'(', (char16_t)0x8169},
	    {')', (char16_t)0x816a}, {'*', (char16_t)0x8196}, {'+', (char16_t)0x817b},  {',', (char16_t)0x8143},
	    {'-', (char16_t)0x817c}, {'.', (char16_t)0x8144}, {'/', (char16_t)0x815e},  {':', (char16_t)0x8146},
	    {';', (char16_t)0x8147}, {'<', (char16_t)0x8183}, {'=', (char16_t)0x8181},  {'>', (char16_t)0x8184},
	    {'?', (char16_t)0x8148}, {'@', (char16_t)0x8197}, {'[', (char16_t)0x816d},  {'\\', (char16_t)0x815f},
	    {']', (char16_t)0x816e}, {'^', (char16_t)0x814f}, {'_', (char16_t)0x8151},  {'`', (char16_t)0x814d},
	    {'{', (char16_t)0x816f}, {'|', (char16_t)0x8162}, {'}', (char16_t)0x8170},  {'~', (char16_t)0x8160},
	};

	int pos = 0;
	while (pos < input.length())
	{
		auto c = input[pos];
		if ((u8)(0x80 & (u8)c) == 0x80)
		{
			// This is a 2 char rune, move to next
			pos += 2;
			continue;
		}

		bool hasConversion = specialCharConvert.count(c);
		if (!hasConversion)
		{
			pos += 1;
			continue;
		}

		// Remove previous character
		input.erase(pos, 1);

		// Add new chars to pos to replace
		auto newChars = (char *)&specialCharConvert[c];
		input.insert(input.begin() + pos, 1, newChars[0]);
		input.insert(input.begin() + pos, 1, newChars[1]);
	}
}

std::string ConvertStringForGame(const std::string &input, int length)
{
	auto utf32 = UTF8ToUTF32(input);

	// Limit length
	if (utf32.length() > length)
	{
		utf32.resize(length);
	}

	auto utf8 = UTF32toUTF8(utf32);
	auto shiftJis = UTF8ToSHIFTJIS(utf8);
	ConvertNarrowSpecialSHIFTJIS(shiftJis);

	// Make fixed size
	shiftJis.resize(length * 2 + 1);
	return shiftJis;
}

#ifdef _WIN32

std::string UTF16ToUTF8(const std::wstring& input)
{
	auto const size = WideCharToMultiByte(CP_UTF8, 0, input.data(), (int)input.size(), nullptr, 0,
		nullptr, nullptr);

	std::string output;
	output.resize(size);

	if (size == 0 ||
		size != WideCharToMultiByte(CP_UTF8, 0, input.data(), (int)input.size(), &output[0],
		(int)output.size(), nullptr, nullptr))
	{
		output.clear();
	}

	return output;
}

std::wstring CPToUTF16(u32 code_page, const std::string& input)
{
	auto const size = MultiByteToWideChar(code_page, 0, input.data(), (int)input.size(), nullptr, 0);

	std::wstring output;
	output.resize(size);

	if (size == 0 ||
		size != MultiByteToWideChar(code_page, 0, input.data(), (int)input.size(), &output[0],
		(int)output.size()))
	{
		output.clear();
	}

	return output;
}

std::string UTF16ToCP(u32 code_page, const std::wstring &input)
{
	std::string output;

	if (0 != input.size())
	{
		// "If cchWideChar [input buffer size] is set to 0, the function fails." -MSDN
		auto const size = WideCharToMultiByte(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0,
		                                      nullptr, nullptr);

		output.resize(size);

		if (size != WideCharToMultiByte(code_page, 0, input.data(), static_cast<int>(input.size()), &output[0],
		                                static_cast<int>(output.size()), nullptr, nullptr))
		{
			const DWORD error_code = GetLastError();
			ERROR_LOG(COMMON, "WideCharToMultiByte Error in String '%s': %lu", std::wstring(input).c_str(), error_code);
			output.clear();
		}
	}

	return output;
}

std::wstring UTF8ToUTF16(const std::string& input)
{
	return CPToUTF16(CP_UTF8, input);
}

std::string SHIFTJISToUTF8(const std::string& input)
{
	return UTF16ToUTF8(CPToUTF16(CODEPAGE_SHIFT_JIS, input));
}

std::string UTF8ToSHIFTJIS(const std::string &input)
{
	return UTF16ToCP(CODEPAGE_SHIFT_JIS, UTF8ToUTF16(input));
}

std::string CP1252ToUTF8(const std::string& input)
{
	return UTF16ToUTF8(CPToUTF16(1252, input));
}

std::u32string UTF8ToUTF32(const std::string &input)
{
	std::wstring_convert<std::codecvt_utf8<int32_t>, int32_t> utf32Convert;
	auto asInt = utf32Convert.from_bytes(input);
	return std::u32string(reinterpret_cast<char32_t const *>(asInt.data()), asInt.length());
}

std::string UTF32toUTF8(const std::u32string &input)
{
	std::wstring_convert<std::codecvt_utf8<int32_t>, int32_t> utf8Convert;
	auto p = reinterpret_cast<const int32_t *>(input.data());
	return utf8Convert.to_bytes(p, p + input.size());
}
#else
template <typename T>
std::string CodeTo(const char *tocode, const char *fromcode, const std::basic_string<T>& input)
{
	std::string result;

	iconv_t const conv_desc = iconv_open(tocode, fromcode);
	if ((iconv_t)-1 == conv_desc)
	{
		ERROR_LOG(COMMON, "Iconv initialization failure [%s]: %s", fromcode, strerror(errno));
	}
	else
	{
		size_t const in_bytes = sizeof(T) * input.size();
		size_t const out_buffer_size = 4 * in_bytes;

		std::string out_buffer;
		out_buffer.resize(out_buffer_size);

		auto src_buffer = &input[0];
		size_t src_bytes = in_bytes;
		auto dst_buffer = &out_buffer[0];
		size_t dst_bytes = out_buffer.size();

		while (src_bytes != 0)
		{
			size_t const iconv_result =
				iconv(conv_desc, (char**)(&src_buffer), &src_bytes, &dst_buffer, &dst_bytes);

			if ((size_t)-1 == iconv_result)
			{
				if (EILSEQ == errno || EINVAL == errno)
				{
					// Try to skip the bad character
					if (src_bytes != 0)
					{
						--src_bytes;
						++src_buffer;
					}
				}
				else
				{
					ERROR_LOG(COMMON, "iconv failure [%s]: %s", fromcode, strerror(errno));
					break;
				}
			}
		}

		out_buffer.resize(out_buffer_size - dst_bytes);
		out_buffer.swap(result);

		iconv_close(conv_desc);
	}

	return result;
}

template <typename T>
std::string CodeToUTF8(const char* fromcode, const std::basic_string<T>& input)
{
	return CodeTo("UTF-8", fromcode, input);
}

std::string CP1252ToUTF8(const std::string& input)
{
	// return CodeToUTF8("CP1252//TRANSLIT", input);
	// return CodeToUTF8("CP1252//IGNORE", input);
	return CodeToUTF8("CP1252", input);
}

std::string SHIFTJISToUTF8(const std::string& input)
{
	// return CodeToUTF8("CP932", input);
	return CodeToUTF8("SJIS", input);
}

std::string UTF8ToSHIFTJIS(const std::string& input)
{
	return CodeTo("SJIS", "UTF-8", input);
}

std::string UTF16ToUTF8(const std::wstring& input)
{
	std::string result = CodeToUTF8("UTF-16LE", input);

	// TODO: why is this needed?
	result.erase(std::remove(result.begin(), result.end(), 0x00), result.end());
	return result;
}

std::u32string UTF8ToUTF32(const std::string &input)
{
	auto val = CodeTo("UTF-32LE", "UTF-8", input);
	auto utf32Data = (char32_t*)val.data();
	return std::u32string(utf32Data, utf32Data + (val.size() / 4));
}

std::string UTF32toUTF8(const std::u32string &input)
{
	auto utf8Data = (char*)input.data();
	auto str = std::string(utf8Data, utf8Data + (input.size() * 4));
	return CodeTo("UTF-8", "UTF-32LE", str);
}

#endif

std::string ValueToString(u32 value)
{
  return StringFromFormat("0x%08x", value);
}

std::string ValueToString(float value)
{
  return StringFromFormat("%#.9g", value);
}

std::string ValueToString(double value)
{
  return StringFromFormat("%#.17g", value);
}

std::string ValueToString(int value)
{
  return std::to_string(value);
}

std::string ValueToString(bool value)
{
  return value ? "True" : "False";
}