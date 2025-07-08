#include "StringPool.h"

namespace PinInCpp {
	UTF8StringPool::UTF8StringPool() {
		chars_offset.push_back(0);//初始化时在开头添加0作为元素，可以避免if检查上一个元素是否越界
		strs_offset.push_back(0);
	}

	size_t UTF8StringPool::put(const std::string& s) {
		strs.insert(strs.end(), s.begin(), s.end());//数据插入

		Utf8String utf8s(s);
		strs_offset.push_back(strs_offset[strs_offset.size() - 1] + utf8s.size() + 1);
		for (const auto& str : utf8s) {
			chars_offset.push_back(chars_offset[chars_offset.size() - 1] + str.size());
		}

		strs.push_back('\0');
		chars_offset.push_back(chars_offset[chars_offset.size() - 1] + 1);//结尾符的宽度
		return strs_offset[strs_offset.size() - 2];
	}

	std::string UTF8StringPool::getchar(size_t i)const {
		i++;//+1让原本效果为1起始的变为0起始

		size_t size = chars_offset[i];
		size_t last = chars_offset[i - 1];

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

	size_t UTF8StringPool::getLastStrSize()const {
		size_t size = strs_offset.size();
		return strs_offset[size - 1] - strs_offset[size - 2] - 1;//-1是为了不算上结尾符
	}
}
