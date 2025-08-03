#include "StringPool.h"

namespace PinInCpp {
	size_t UTF8StringPool::put(const std::string_view& s) {
		strs.insert(strs.end(), s.begin(), s.end());//数据插入
		strs.emplace_back('\0');

		size_t result = last_offset;

		size_t cursor = 0;
		size_t end = s.size();
		size_t lastCharsSize = chars_offset.size() - 1;
		size_t currentLastOffset = last_offset;//走局部更快，不用走this指针
		while (cursor < end) {
			size_t charSize = getUTF8CharSize(s[cursor]);
			chars_offset.emplace_back(chars_offset[lastCharsSize] + charSize);
			cursor += charSize;
			lastCharsSize++;
			currentLastOffset++;
		}
		last_offset = currentLastOffset + 1;//空字符也有呢

		chars_offset.emplace_back(chars_offset[chars_offset.size() - 1] + 1);//结尾符的宽度
		return result;
	}

	std::string UTF8StringPool::getchar(size_t i)const {
		size_t size = chars_offset[i + 1];
		size_t last = chars_offset[i];

		std::string result;
		result.insert(result.end(), strs.begin() + last, strs.begin() + size);

		return result;
	}

	std::string UTF8StringPool::getstr(size_t strStart)const {
		std::string result;

		size_t i = chars_offset[strStart];
		while (strs[i]) {
			result.push_back(strs[i]);
			i++;
		}
		return result;
	}

	std::string_view UTF8StringPool::getchar_view(size_t i)const noexcept {
		size_t size = chars_offset[i + 1];
		size_t last = chars_offset[i];

		return std::string_view(strs.data() + last, size - last);
	}

	std::string_view UTF8StringPool::getstr_view(size_t strStart)const noexcept {
		strStart = chars_offset[strStart];
		size_t i = strStart;
		while (strs[i]) {
			i++;
		}
		return std::string_view(strs.data() + strStart, i - strStart);
	}

	uint32_t UTF8StringPool::getcharFourCC(size_t i)const noexcept {
		size_t end = chars_offset[i + 1];
		size_t start = chars_offset[i];
		uint32_t result = 0;

		for (size_t i = start; i < end; i++) {
			result <<= 8;
			result |= (uint8_t)strs[i];
		}
		return result;
	}
	bool UTF8StringPool::EqualChar(size_t indexA, size_t indexB)const noexcept {
		if (indexA == indexB) {//两个索引相等，那么就是同一个字符，必然相等
			return true;
		}
		size_t AOffset = chars_offset[indexA];
		size_t BOffset = chars_offset[indexB];
		size_t Asize = chars_offset[indexA + 1] - AOffset;
		size_t Bsize = chars_offset[indexB + 1] - BOffset;
		if (Asize != Bsize) {//两个字节大小都不一样，那肯定不相等
			return false;
		}
		for (size_t i = 0; i < Asize; i++) {
			if (strs[AOffset + i] != strs[BOffset + i]) {
				return false;
			}
		}
		return true;
	}
}
