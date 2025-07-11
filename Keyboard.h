#pragma once
#include <map>
#include <string>
#include <optional>
#include <functional>
#include <iostream>
/*
	拼音上下文是带声调的！
	拼音字符应当都是ASCII可表示的字符，不然字符串处理会出问题
*/

namespace PinInCpp {
	bool hasInitial(const std::string_view& s);//本质上就是一个处理函数，直接放这里也没啥

	using OptionalStrMap = std::optional<std::map<std::string, std::string>>;
	using CutterFn = std::function<std::vector<std::string_view>(const std::string_view&)>;//应该只处理音节，无需处理音调，自定义逻辑的时候请注意！和原始Java项目行为不一致

	class Keyboard {
	public:
		Keyboard(const OptionalStrMap& MapLocalArg, const OptionalStrMap& MapKeysArg, CutterFn cutter, bool duo, bool sequence)
			:cutter{ cutter }, duo{ duo }, sequence{ sequence } {
			//在插入完成数据之前，构建视图都是不安全的行为，因为容器可能会随时扩容
			//所以需要缓存数据，在插入完成后再根据数据构建视图
			std::vector<InsertStrData> MapLocalData;
			std::vector<InsertStrData> MapKeysData;
			if (MapLocalArg != std::nullopt) {//不为空
				InsertDataFn(MapLocalArg, MapLocalData);
			}
			if (MapKeysArg != std::nullopt) {
				InsertDataFn(MapKeysArg, MapKeysData);
			}
			//数据均插入完成了，可以开始构建视图了
			if (!MapLocalData.empty()) {//检查容器是否为空，防止map里也是空的情况下构造了map，因为那样是无用的
				MapLocal = std::map<std::string_view, std::string_view>();//构建！
				CreateViewOnMap(MapLocal.value(), MapLocalData);
			}
			if (!MapKeysData.empty()) {
				MapKeys = std::map<std::string_view, std::string_view>();
				CreateViewOnMap(MapKeys.value(), MapKeysData);
			}
		}
		virtual ~Keyboard() = default;

		std::string_view keys(const std::string_view& s)const;
		std::vector<std::string_view> split(const std::string_view& s)const;

		static std::vector<std::string_view> standard(const std::string_view& s);
		static std::vector<std::string_view> zero(const std::string_view& s);

		static const Keyboard QUANPIN;//类内不完整，所以这些的构造放cpp文件了
		static const Keyboard DAQIAN;
		static const Keyboard XIAOHE;
		static const Keyboard ZIRANMA;
		static const Keyboard SOUGOU;
		static const Keyboard GUOBIAO;
		static const Keyboard MICROSOFT;
		static const Keyboard PINYINPP;
		static const Keyboard ZIGUANG;

		bool duo;
		bool sequence;
	private:
		struct InsertStrData {
			size_t keySize;
			size_t keyStart;

			size_t valueSize;
			size_t valueStart;
		};
		//DRY!
		void InsertDataFn(const OptionalStrMap& srcData, std::vector<InsertStrData>& data) {
			for (const auto& [key, value] : srcData.value()) {
				size_t keySize = key.size();
				size_t keyStart = pool.put(key);

				size_t valueSize = value.size();
				size_t valueStart = pool.put(value);
				data.push_back({ keySize,keyStart,valueSize,valueStart });
			}
		}
		void CreateViewOnMap(std::map<std::string_view, std::string_view>& Target, const std::vector<InsertStrData>& data) {
			char* poolptr = pool.data();
			for (const auto& data : data) {
				std::string_view key(poolptr + data.keyStart, data.keySize);
				std::string_view value(poolptr + data.valueStart, data.valueSize);
				Target[key] = value;
			}
		}
		//不是StringPoolBase的派生类，是用于Keyboard持有字符串生命周期的内存池
		class StrPool {
		public: //作为字符串视图的数据源，他不需要终止符
			size_t put(const std::string& str) {
				size_t result = strs.size();
				strs.insert(strs.end(), str.begin(), str.end());
				return result;
			}
			char* data() {
				return strs.data();
			}
		private:
			std::vector<char> strs;
		};
		using OptionalStrViewMap = std::optional<std::map<std::string_view, std::string_view>>;
		StrPool pool;
		OptionalStrViewMap MapLocal;
		OptionalStrViewMap MapKeys;
		CutterFn cutter = standard;
	};
}
