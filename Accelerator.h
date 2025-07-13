#pragma once
#include <set>

#include "StringPool.h"

namespace PinInCpp {
	class Accelerator {
	public:
		Accelerator(const PinIn& p) : ctx{ p } {
		}
		void search(const std::string& s) {
			if (s != searchStr.ToStream()) {
				searchStr = s;
				reset();
			}
		}
		void reset() {
			cache.clear();
		}
		//接收一个外部的、长生命周期的provider
		void setProvider(StringPoolBase* provider_ptr) {
			provider = provider_ptr;
			owned_provider.reset(); //置空另一个
		}
		//字符串版本
		void setProvider(const std::string& s) {
			provider = nullptr; //置空另一个
			owned_provider.emplace(s); //在自己的 optional 中创建一个拥有数据的副本
		}
		IndexSet get(const PinIn::Pinyin& p, size_t offset);
		IndexSet get(const std::string& ch, size_t offset);
		size_t common(size_t s1, size_t s2, size_t max);
		bool check(size_t offset, size_t start);
		bool matches(size_t offset, size_t start);
		bool begins(size_t offset, size_t start);
		bool contains(size_t offset, size_t start);
		const Utf8String& getSearchStr() {
			return searchStr;
		}
	private:
		//两个数据源成员，互斥存在
		StringPoolBase* provider = nullptr;     //观察者指针，不拥有
		std::optional<Utf8String> owned_provider; //拥有型，用于存储临时字符串的解析结果

		const PinIn& ctx;

		std::vector<IndexSet::Storage> cache;
		Utf8String searchStr;
		bool partial = false;
	};
}
