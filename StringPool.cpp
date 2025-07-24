#include "StringPool.h"

namespace PinInCpp {
	UTF8StringPool::UTF8StringPool() {
		chars_offset.push_back(0);//初始化时在开头添加0作为元素，可以避免if检查上一个元素是否越界
		//strs_offset.push_back(0);
	}
	/*
	size_t UTF8StringPool::put(const std::string& s) {
		strs.insert(strs.end(), s.begin(), s.end());//数据插入

		Utf8StringView utf8s(s);
		strs_offset.push_back(strs_offset[strs_offset.size() - 1] + utf8s.size() + 1);
		for (const auto& str : utf8s) {
			chars_offset.push_back(chars_offset[chars_offset.size() - 1] + str.size());
		}

		strs.push_back('\0');
		chars_offset.push_back(chars_offset[chars_offset.size() - 1] + 1);//结尾符的宽度
		return strs_offset[strs_offset.size() - 2];
	}*/

	size_t UTF8StringPool::put(const std::string_view& s) {
		strs.insert(strs.end(), s.begin(), s.end());//数据插入

		Utf8StringView utf8s(s);
		last_size = utf8s.size();
		size_t result = last_offset;
		for (const auto& str : utf8s) {
			last_offset++;
			chars_offset.push_back(chars_offset[chars_offset.size() - 1] + str.size());
		}
		last_offset++;//空字符也有呢
		strs.push_back('\0');
		chars_offset.push_back(chars_offset[chars_offset.size() - 1] + 1);//结尾符的宽度
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

	std::string_view UTF8StringPool::getchar_view(size_t i)const {
		size_t size = chars_offset[i + 1];
		size_t last = chars_offset[i];

		return std::string_view(strs.data() + last, size - last);
	}

	std::string_view UTF8StringPool::getstr_view(size_t strStart)const {
		strStart = chars_offset[strStart];
		size_t i = strStart;
		while (strs[i]) {
			i++;
		}
		return std::string_view(strs.data() + strStart, i - strStart);
	}

	uint32_t UTF8StringPool::getcharFourCC(size_t i)const {
		size_t size = chars_offset[i + 1];
		size_t last = chars_offset[i];
		uint32_t result = 0;

		size -= last;
		for (size_t i = 0; i < size; i++) {
			result <<= 8;
			result |= (uint8_t)strs[last + i];
		}
		return result;
	}
	bool UTF8StringPool::EqualChar(size_t indexA, size_t indexB)const {
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
