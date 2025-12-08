#pragma once
#include "StringPool.h"
#include "PinIn4Cpp/PinIn.h"

namespace PinInCpp {
	namespace detail {
		class Accelerator {
		public:
			Accelerator(PinIn& p) : ctx{ p } {
			}
			const Utf8StringView& search() {
				return searchStr;
			}
			uint32_t searchU32FourCC(size_t i) {
				return u32strVec[i];
			}
			void search(std::string_view s) {
				if (s != searchSrcStr) {
					searchSrcStr = s;
					searchStr.reset(searchSrcStr);
					if (cache.size() < searchStr.size()) {
						cache.resize(searchStr.size());
					}

					u32strVec.clear();
					for (const auto& v : searchStr) {
						u32strVec.emplace_back(FourCCToU32(v));
					}
					reset();
				}
			}
			void reset() {
				for (auto& v : cache) {
					v.clear();//仅清空map容器，避免map对象再构造
				}
			}
			//接收一个外部的、长生命周期的provider，不拥有
			void setProvider(UTF8StringPool* provider_ptr) {
				provider = provider_ptr;
			}
			void ShrinkToFit() {
				u32strVec.shrink_to_fit();
				searchStr.ShrinkToFit();
				searchSrcStr.shrink_to_fit();
				cache.shrink_to_fit();
			}

			IndexSet get(const PinIn::Pinyin& p, size_t offset);
			IndexSet get(const uint32_t ch, size_t offset);
			size_t common(size_t s1, size_t s2, size_t max);
			bool check(size_t offset, size_t start);
			bool matches(size_t offset, size_t start);
			bool begins(size_t offset, size_t start);
			bool contains(size_t offset, size_t start);
			const Utf8StringView& getSearchStr() {
				return searchStr;
			}
		private:
			UTF8StringPool* provider = nullptr;     //观察者指针，不拥有

			PinIn& ctx;
			std::vector<IndexSet::Storage> cache;
			Utf8StringView searchStr;
			std::string searchSrcStr;
			std::vector<uint32_t> u32strVec;
			bool partial = false;
		};
	}
}
