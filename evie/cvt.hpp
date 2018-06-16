/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once
#include "myitoa.hpp"

inline size_t check_utf8_get_length(const char *cp, size_t length)
{
    size_t l;
    size_t n = 0;

    for (l=0; l < length; ++n)
    {
        if ((cp[l] & 0x80) == 0)
        {
            ++l;
            continue;
        }

        if ((cp[l] & 0xE0) == 0xC0)
        {
            if ((cp[l+1] & 0xC0) == 0x80)
            {
                l += 2;
                continue;
            }
        }

        if ((cp[l] & 0xF0) == 0xE0)
        {
            if ((cp[l+1] & 0xC0) == 0x80 &&
                (cp[l+2] & 0xC0) == 0x80)
            {
                l += 3;
                continue;
            }
        }

        throw std::runtime_error("bad utf8 string");
    }
    return n;
}

inline std::string ucs2_to_utf8(const wchar_t *source, size_t length)
{
    wchar_t uc;
    int len = 0;

    if (length == 0)
        length = wcslen(source);

    const wchar_t* src = source;
    const wchar_t* end = source + length;

    while (src != end) {
        uc = *src++;
        if (uc == (uc & 0x007F))
            len += 1;
        else if (uc == (uc & 0x07FF))
            len += 2;
        else
            len += 3;
    }

    std::string dst;
    dst.resize(len);
    src = source;
    int inx = 0;

    while (src != end) {
        uc = *src++;

        if (uc == (uc & 0x007F))
            dst[inx++] = (char)uc;
        else if (uc == (uc & 0x07FF)) {
            dst[inx+1] = (char)(uc & 0x3F) | 0x80;
            uc >>= 6;
            dst[inx]   = (char)(uc & 0x1F) | 0xC0;
            inx += 2;
            continue;
        } else {
            dst[inx+2] = (char)(uc & 0x3F) | 0x80;
            uc >>= 6;
            dst[inx+1] = (char)(uc & 0x3F) | 0x80;
            uc >>= 6;
            dst[inx]   = (char)(uc & 0x0F) | 0xE0;
            inx += 3;
        }
    }
    return dst;
}

inline std::wstring utf8_to_ucs2(const char *source, size_t slength)
{
    size_t len = check_utf8_get_length(source, slength);

    size_t l;
    size_t n = 0;

    std::wstring p;
    p.resize(len);

    wchar_t uc;

    for (l=0; l < slength; p[n++] = uc)
    {
        if ((source[l] & 0x80) == 0)
            uc = source[l++];
        else if ((source[l] & 0xE0) == 0xC0 && (source[l+1] & 0xC0) == 0x80) {

            uc = source[l] & 0x1F;
            uc <<= 6; uc |= source[l+1] & 0x3F;
            l += 2;

        } else if ((source[l] & 0xF0)   == 0xE0 &&
            (source[l+1] & 0xC0) == 0x80 &&
            (source[l+2] & 0xC0) == 0x80) {

                uc = source[l] & 0x0F;
                uc <<= 6; uc |= source[l+1] & 0x3F;
                uc <<= 6; uc |= source[l+2] & 0x3F;
                l += 3;
        } else {
            uc = source[l];
            ++l;
        }
    }
    return p;
}

inline std::wstring utf8_to_ucs2(const std::string &source)
{
    return utf8_to_ucs2(source.c_str(), source.size());
}
inline std::string ucs2_to_utf8(const std::wstring &source)
{
    return ucs2_to_utf8(source.c_str(), source.size());
}

inline std::wstring StrToWstr(const std::string& str)
{
    if(str.empty()) return std::wstring();
    std::vector<wchar_t> tmp(str.size());
#ifdef WIN32
    size_t ret = _mbstowcs_l(&tmp[0], str.c_str(), str.size(), _create_locale(LC_CTYPE,"Russian"));
#else
    size_t ret = mbstowcs(0, str.c_str(), 0);
    if(ret != size_t(-1)){
        tmp.resize(ret);
        ret = mbstowcs(&tmp[0], str.c_str(), tmp.size());
    }
#endif
    if(ret == size_t(-1))
        throw std::runtime_error(es() % "StrToWstr error, with input string: " % str);
    return std::wstring(tmp.begin(), tmp.end());
}

inline std::string CurLocalToUtf8(const std::string& str)
{
    std::wstring ret = StrToWstr(str);
    return ucs2_to_utf8(ret);
}

inline wchar_t cp1251ToUc2(const unsigned char c){
	//for other symbols
	//http://ru.wikipedia.org/wiki/%D0%9A%D0%B8%D1%80%D0%B8%D0%BB%D0%BB%D0%B8%D1%86%D0%B0_%D0%B2_%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%D0%B5

	wchar_t ret = 0;
	if(c == 184) //¸
		return 0x0451;
	if(c == 168) //¨
		return 0x0401;
	
	if(c >= 0xc0)
		ret = 0x350 + c;
	else return c;
	return ret;
}

template<typename T>
inline void WriteUc2ToUtfStream(wchar_t c, T& str){
    if (c == (c & 0x007F))
        str.push_back(char(c));
    else if (c == (c & 0x07FF)){
        char cm = c >> 6;
        str.push_back((char)(cm & 0x1F) | 0xC0);
        str.push_back((char)(c & 0x3F) | 0x80);
    } else{
        char cm = c >> 6;
        char cm2 = cm >> 6;

        str.push_back((char)(cm2 & 0x0F) | 0xE0);
        str.push_back((char)(cm & 0x3F) | 0x80);
        str.push_back((char)(c & 0x3F) | 0x80);
    }
}

template<typename string>
inline std::string CvtCp1251ToUtf8(const string& str){
    auto it = str.begin(), it_e = str.end();
    std::string ret;
    ret.reserve(str.size() * 3);
    for(; it != it_e; ++it){
        wchar_t s = cp1251ToUc2(*it);
        WriteUc2ToUtfStream(s, ret);
    }
    return ret;
}

template <typename Str>
inline std::wstring CvtCp1251ToUc2(const Str& str){
    std::wstring ret;
    std::transform(str.begin(), str.end(), std::back_inserter(ret), &cp1251ToUc2);
    return ret;
}
inline std::string CvtUc2ToCp1251(const std::wstring &str, bool throw_en = true)
{
    std::string ret;
    ret.reserve(str.size());
    std::wstring::const_iterator it = str.begin(), it_e = str.end();
    for(; it != it_e; ++it){
        wchar_t s = *it;
        char c = 0;
        if(s == 0x0451)
            c = char(184);
        else if(s == 0x0401)
            c = char(168);
        else if(s >= 0x350 + 0xc0 && s <= 0x350 + 0xff)
            c = char(s - 0x350);
        else if(s < 0xc0)
            c = char(s);
        else if(s == 8470)
            c = '¹';
        else if(throw_en)
            throw std::runtime_error(es() % "Bad symbol " % s % " in string for CvtUc2ToCp1251");
        if(c)
            ret.push_back(c);
    }
    return ret;
}
inline std::string CvtUtf8ToCp1251(const std::string &str, bool throw_en = true)
{
    std::wstring ucs2 = utf8_to_ucs2(str);
    return CvtUc2ToCp1251(ucs2, throw_en);
}
inline bool isBadSmb(char c){
    if((c >= 0x0 && c <= 0x1F) && c != '\t' && c != '\n')
        return true;
    return false;
}
inline char hexit(char c) {
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    throw std::runtime_error("hexit error");
}
inline std::string urldecode(const std::string& str){
    std::string ret;
    std::string::const_iterator it = str.begin(), it_e = str.end();
    for(; it != it_e; ++it){
        if(*it == '%'){
            if(it_e - it < 2)
                throw std::runtime_error(es() % "urldecode string error: " % str);
            ++it;
            char c = hexit(*it) * 16 + hexit(*(it + 1));
            ++it;
            ret.push_back(c);
        }
        else
            ret.push_back(*it);
    }
    return ret;
}
inline std::string urlencode(const std::string & str)
{
    const char hexchars[] = "0123456789ABCDEF";
    std::string res;
    for (int x = 0, y = 0, len = str.length(); len--; x++, y++)
    {
        res += (unsigned char) str[x];
        if (res[y] == ' ') res[y] = '+';
        else if (!((res[y] >= '0' && res[y] <= '9')
            || (res[y] >= 'A' && res[y] <= 'Z')
            || (res[y] >= 'a' && res[y] <= 'z')
            || res[y] == '-' || res[y] == '.' || res[y] == '_'))
        {
            // Allow only alphanumeric chars and '_', '-', '.'; escape the rest
            res[y] = '%';
            res += hexchars[(unsigned char) str[x] >> 4];
            ++y;
            res += hexchars[(unsigned char) str[x] & 0x0F];
            ++y;
        }
    }
    return res;
}

const char cp1251_from[] =
"ÉÖÓÊÅÍÃØÙÇÕÚÔÛÂÀÏĞÎËÄÆİß×ÑÌÈÒÜÁŞéöóêåíãøùçõúôûâàïğîëäæıÿ÷ñìèòüáş";
const char cp1251_to[] =
"YCUKENGSSZH'FIVAPROLDJAR4SMIT'BU"
"ycukengsszh'fivaproldjaR4smit'bu";
static_assert(sizeof(cp1251_from) == sizeof(cp1251_to), "cp1251sz");

static void init_cp1251_to_latin(char (&buf) [256])
{
    const char* to = cp1251_from + sizeof(cp1251_from);
    for(int i = 0; i != sizeof(buf); ++i){
        const char* it = std::find(cp1251_from, to, char(i));
        if(it == to){
            buf[i] = char(i);
        }
        else
            buf[i] = cp1251_to[it - cp1251_from];
    }
}

