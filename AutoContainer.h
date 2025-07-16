#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <functional>

namespace PinInCpp {
	constexpr int CONTAINER_THRESHOLD = 128;


	template<typename key, typename value>
	class Obj2ObjMap {
	public:

	private://默认一开始是数组模式，然后是插入过多数据后变成unordered容器
		class AbstractMap {
		public:


		};
		class ArrayMap : public AbstractMap {
		public:

		private:
			std::vector<key> keyData;
			std::vector<value> valueData;
		};
	};

	template<typename value>
	class ObjSet {//集合不承担查找，这个就很简单
	private:
		class AbstractSet {
		public:
			virtual AbstractSet* insert(const value& input_v) = 0;
			virtual void AddToSTLSet(std::unordered_set<value>& input_v) = 0;//有点反客为主了
		};

		class HashSet : public AbstractSet {
		public:
			virtual AbstractSet* insert(const value& input_v) {
				data.insert(input_v);
				return this;
			}
			virtual void AddToSTLSet(std::unordered_set<value>& input_v) {
				for (const value& v : data) {
					input_v.insert(v);
				}
			}
		private:
			std::unordered_set<value> data;
		};

		class ArraySet : public AbstractSet {
		public:
			virtual AbstractSet* insert(const value& input_v) {
				for (const value& v : data) {
					if (v == input_v) {
						return this;//如果查到，有相等的，则为重复
					}
				}
				data.push_back(input_v);
				if (data.size() > CONTAINER_THRESHOLD) {
					std::unique_ptr<HashSet> result = std::make_unique<HashSet>();
					for (const value& v : data) {
						result->insert(v);
					}
					return result.release();
				}
				return this;
			}
			virtual void AddToSTLSet(std::unordered_set<value>& input_v) {
				for (const value& v : data) {
					input_v.insert(v);
				}
			}
		private:
			std::vector<value> data;
		};

		std::unique_ptr<AbstractSet> Container;
	public:
		ObjSet() :Container{ std::make_unique<ArraySet>() } {}
		ObjSet(std::initializer_list<value> list) :Container{ std::make_unique<ArraySet>() } {
			for (const value& v : list) {
				insert(v);
			}
		}
		void insert(const value& input_v) {
			AbstractSet* set = Container->insert(input_v);
			if (set != Container.get()) {
				Container.reset(set);
			}
		}
		void AddToSTLSet(std::unordered_set<value>& input_v) {
			Container->AddToSTLSet(input_v);
		}
	};
}
