#pragma once
#include <functional>
#include <unordered_map>
#include <iostream>
namespace PinInCpp {
	constexpr static uint32_t IndexSetIterEnd = static_cast<uint32_t>(-1);

	//平凡类型，非常高效
	class IndexSet {
	public:
		IndexSet() = default;
		~IndexSet() = default;
		static IndexSet Init(uint32_t i = 0) {
			IndexSet result = IndexSet();
			result.value = i;
			return result;
		}

		static const IndexSet ZERO;
		static const IndexSet ONE;
		static const IndexSet NONE;

		void set(uint32_t index) {
			value |= (0x1 << index);
		}
		bool get(uint32_t index)const {
			return (value & (0x1 << index)) != 0;
		}
		void merge(const IndexSet s) {//平凡类型本质上可以被优化为基本类型，而基本类型在传递时，值传递比引用传递快
			value = value == 0x1 ? s.value : (value | s.value);
		}
		void offset(int i) {
			value <<= i;
		}
		bool empty()const {
			return value == 0;
		}

		class IndexSetIterObj {//同样是平凡类型，这样设计可以避免std::function的开销
		public:
			uint32_t Next() {
				for (; count < 32; count++) {
					if ((value & 0x1) == 0x1) {
						value >>= 1;
						uint32_t result = count;
						count++;
						return result;
					}
					else if (value == 0) {
						return IndexSetIterEnd;
					}
					value >>= 1;
				}
				return IndexSetIterEnd;
			}
			static IndexSetIterObj Init(uint32_t i) {
				IndexSetIterObj result = IndexSetIterObj();
				result.value = i;
				result.count = 0;
				return result;
			}
		private:
			uint32_t value;
			uint8_t count;
		};

		IndexSetIterObj GetIterObj()const {
			return IndexSetIterObj::Init(value);
		}

		class Storage {
		public:
			void set(const IndexSet is, uint32_t index) {//同merge方法注释
				data.insert_or_assign(index, is.value + 1);
			}
			IndexSet get(size_t index) {
				auto it = data.find(index);
				if (it == data.end() || it->second == 0) {
					return IndexSet::Init();//空IndexSet也可以代表空元素，而且因为只有uint32_t成员，根本不耗性能
				}
				return IndexSet::Init(it->second - 1);
			}
		private:
			std::unordered_map<size_t, uint32_t>data;
		};
	private:
		uint32_t value;
		friend bool operator==(IndexSet a, IndexSet b);
	};

	inline bool operator==(IndexSet a, IndexSet b) {
		return a.value == b.value;
	}
}
