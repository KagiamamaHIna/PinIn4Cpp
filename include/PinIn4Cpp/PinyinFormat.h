#pragma once
/*
	非必须，且没什么用
*/
#include "PinIn.h"

namespace PinInCpp {
	enum class PinyinFormatEnum : char {
		FORMAT_RAW, FORMAT_NUMBER, FORMAT_PHONETIC, FORMAT_UNICODE
	};
	std::string PinyinFormat(const PinIn::Pinyin& p, PinyinFormatEnum FormatType);
}
