#include "Accelerator.h"

namespace PinInCpp {
	inline static bool utf8_string_end(const Utf8String& str, size_t index) {
		return index >= str.size();
	}

	IndexSet Accelerator::get(const PinIn::Pinyin& p, int offset) {
		if (cache.size() <= offset) {//检查是否过小
			cache.resize(offset + 1);//过小触发resize，重设置大小
		}
		IndexSet::Storage& data = cache[offset];
		IndexSet ret = data.get(p.id);
		if (ret == IndexSet::NONE) {
			ret = p.match(searchStr.ToStream(), offset, partial);
			data.set(ret, static_cast<uint32_t>(p.id));
		}
		return ret;
	}

	IndexSet Accelerator::get(const std::string& ch, int offset) {
		PinIn::Character c = ctx.GetChar(ch);
		IndexSet ret = (searchStr[offset] == c.get() ? IndexSet::ONE : IndexSet::NONE).copy();
		for (const PinIn::Pinyin& p : c.GetPinyins()) {
			ret.merge(get(p, offset));
		}
		return ret;
	}

	size_t Accelerator::common(size_t s1, size_t s2, size_t max) {
		if (provider) {//逻辑分支
			for (size_t i = 0; i < max; i++) {//限定循环范围，跳出后返回max，和原始代码同逻辑
				if (provider->end(s1 + i)) {//查询到结尾时强制退出，也是空检查置前
					return i;
				}
				std::string_view a = provider->getchar_view(s1 + i);//只读不写，安全的
				std::string_view b = provider->getchar_view(s2 + i);
				if (a != b) return i;
			}
		}
		else if (owned_provider) {
			const Utf8String& str = owned_provider.value();
			for (size_t i = 0; i < max; i++) {//限定循环范围，跳出后返回max，和原始代码同逻辑
				if (s1 + i >= str.size()) {//查询到结尾时强制退出，也是空检查置前
					return i;
				}
				if (str[s1 + i] != str[s2 + i]) return i;
			}
		}
		return max;
	}
}
