#include "Accelerator.h"

namespace PinInCpp {
	inline static bool utf8_string_end(const Utf8String& str, size_t index)noexcept {
		return index >= str.size();
	}

	IndexSet Accelerator::get(const PinIn::Pinyin& p, size_t offset) {
		if (cache.size() <= offset) {//检查是否过小
			cache.resize(offset + 1);//过小触发resize，重设置大小
		}
		IndexSet::Storage& data = cache[offset];
		IndexSet ret = data.get(p.id);
		if (ret == IndexSet::NONE) {
			ret = p.match(searchStr, offset, partial);
			data.set(ret, static_cast<uint32_t>(p.id));
		}

		return ret;
	}

	IndexSet Accelerator::get(const uint32_t ch, size_t offset) {
		PinIn::Character* c = ctx.GetCharCachePtr(ch);
		if (c == nullptr) {
			PinIn::Character c = ctx.GetChar(ch);
			IndexSet ret = u32strVec[offset] == ch ? IndexSet::ONE : IndexSet::NONE;
			for (const PinIn::Pinyin& p : c.GetPinyins()) {
				ret.merge(get(p, offset));
			}
			return ret;
		}
		else {
			IndexSet ret = u32strVec[offset] == ch ? IndexSet::ONE : IndexSet::NONE;
			for (const PinIn::Pinyin& p : c->GetPinyins()) {
				ret.merge(get(p, offset));
			}
			return ret;
		}
	}

	size_t Accelerator::common(size_t s1, size_t s2, size_t max) {
		for (size_t i = 0; i < max; i++) {//限定循环范围，跳出后返回max，和原始代码同逻辑
			if (provider->end(s1 + i)) {//查询到结尾时强制退出，也是空检查置前
				return i;
			}
			if (!provider->EqualChar(s1 + i, s2 + i)) {
				return i;
			}
		}
		return max;
	}

	bool Accelerator::check(size_t offset, size_t start) {
		if (offset == searchStr.size()) {
			return partial || provider->end(start);
		}
		if (provider->end(start)) {
			return false;
		}
		IndexSet s = get(provider->getcharFourCC(start), offset);//只读不写，安全的

		if (provider->end(start + 1)) {
			size_t i = searchStr.size() - offset;
			return s.get(static_cast<uint32_t>(i));
		}
		else {
			IndexSet::IndexSetIterObj it = s.GetIterObj();
			for (uint32_t i = it.Next(); i != IndexSetIterEnd; i = it.Next()) {
				if (check(offset + i, start + 1)) {
					return true;
				}
			}
		}
		return false;//都没有就给我返回假
	}

	bool Accelerator::matches(size_t offset, size_t start) {
		if (partial) {
			partial = false;
			reset();
		}
		return check(offset, start);
	}

	bool Accelerator::begins(size_t offset, size_t start) {
		if (!partial) {
			partial = true;
			reset();
		}
		return check(offset, start);
	}

	bool Accelerator::contains(size_t offset, size_t start) {
		if (!partial) {
			partial = true;
			reset();
		}
		for (size_t i = start; !provider->end(i); i++) {
			if (check(offset, i)) return true;
		}
		return false;
	}
}
