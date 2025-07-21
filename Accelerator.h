#pragma once
#include "StringPool.h"

namespace PinInCpp {
	class Accelerator {
	public:
		Accelerator(PinIn& p) : ctx{ p } {

		}
		const Utf8String& search() {
			return searchStr;
		}
		const uint32_t searchU32FourCC(size_t i) {
			return u32strVec[i];
		}
		void search(const std::string_view& s) {
			if (s != searchStr.ToStream()) {
				searchStr = std::string(s);
				u32strVec.clear();
				for (const auto& v : searchStr) {
					u32strVec.emplace_back(FourCCToU32(v));
				}
				reset();
			}
		}
		void reset() {
			cache.clear();
		}
		//接收一个外部的、长生命周期的provider，不拥有
		void setProvider(UTF8StringPool* provider_ptr) {
			provider = provider_ptr;
		}

		IndexSet get(const PinIn::Pinyin& p, size_t offset);
		IndexSet get(const uint32_t ch, size_t offset);
		size_t common(size_t s1, size_t s2, size_t max);
		bool check(size_t offset, size_t start);
		bool matches(size_t offset, size_t start);
		bool begins(size_t offset, size_t start);
		bool contains(size_t offset, size_t start);
		const Utf8String& getSearchStr() {
			return searchStr;
		}
	private:
		UTF8StringPool* provider = nullptr;     //观察者指针，不拥有

		PinIn& ctx;
		std::vector<IndexSet::Storage> cache;
		Utf8String searchStr;
		std::vector<uint32_t> u32strVec;
		bool partial = false;
	};
}
