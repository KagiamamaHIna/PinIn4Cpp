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


}
