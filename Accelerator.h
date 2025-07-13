#pragma once

#include <set>

#include "StringPool.h"

namespace PinInCpp {
	class Accelerator {
	public:
		Accelerator(std::unique_ptr<StringPoolBase>& provider, const PinIn& p) :provider{ provider }, ctx{ p } {
		}

		void search(const std::string& s) {
			if (s != searchStr) {
				searchStr = s;
				reset();
			}
		}

		/*
		public IndexSet get(char ch, int offset) {
			Char c = context.getChar(ch);
			IndexSet ret = (searchChars[offset] == c.get() ? IndexSet.ONE : IndexSet.NONE).copy();
			for (Pinyin p : c.pinyins()) ret.merge(get(p, offset));
			return ret;
		}*/

		std::set<size_t> get(const std::vector<std::string>& p, int offset) {
			if (cache.size() <= offset) {//检查是否过小
				cache.resize(offset + 1);//过小触发resize，重设置大小
			}
			std::unique_ptr<std::unordered_map<size_t, size_t>>& data = cache[offset];
			if (data.get() == nullptr) {//避免构造过多对象，给实际需要的分配一个对象 延迟初始化
				data = std::make_unique<std::unordered_map<size_t, size_t>>();
			}
			/*IndexSet ret = data->count(123);
			* 要实现match的逻辑，返回结果集
			if (ret == null) {
				ret = p.match(searchStr, offset, partial);
				data.set(ret, p.id);
			}*/
			return std::set<size_t>();
		}

		void reset() {
			cache.clear();
		}

		size_t common(size_t s1, size_t s2, size_t max) {
			for (size_t i = 0; i < max; i++) {//限定循环范围，跳出后返回max，和原始代码同逻辑
				if (provider->end(s1 + i)) {//查询到结尾时强制退出，也是空检查置前
					return i;
				}
				std::string a = provider->getchar(s1 + i);
				std::string b = provider->getchar(s2 + i);
				if (a != b) return i;
			}
			return max;
		}
	private:
		std::unique_ptr<StringPoolBase>& provider;
		const PinIn& ctx;
		//这个容器是 vector<字符串索引 对 map<拼音id 对 匹配结果>> 后端的size_t类型是不是应该改为std::set<size_t>?
		std::vector<std::unique_ptr<std::unordered_map<size_t, size_t>>> cache;
		std::string searchStr;
		bool partial = false;
	};
}
