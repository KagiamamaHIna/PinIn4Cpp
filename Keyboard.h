#pragma once
#include <map>
#include <string>
#include <optional>
#include <functional>
#include <set>
/*
	拼音上下文是带声调的！
	拼音字符应当都是ASCII可表示的字符，不然字符串处理会出问题
*/

namespace PinInCpp {
	bool hasInitial(const std::string_view& s);//本质上就是一个处理函数，直接放这里也没啥

	using OptionalStrMap = std::optional<std::map<std::string, std::string>>;
	//应该只处理音节，无需处理音调，自定义逻辑的时候请注意！和原始Java项目行为不一致
	//你不应该在里面手动进行音素的改造，比如v->u，他们应该是交由其他的方法处理的，这是纯粹的切割音素函数
	using CutterFn = std::function<std::vector<std::string_view>(const std::string_view&)>;

	class Keyboard {
	public:
		Keyboard(const OptionalStrMap& MapLocalArg, const OptionalStrMap& MapKeysArg, CutterFn cutter, bool duo, bool sequence);
		virtual ~Keyboard() = default;

		std::string_view keys(const std::string_view& s)const;
		std::string_view GetFuzzyPhoneme(const std::string_view& s);
		std::vector<std::string_view> split(const std::string_view& s)const;

		static std::vector<std::string_view> standard(const std::string_view& s);//本身就是一个标准的，处理全拼音素的全局函数
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
		using OptionalStrViewMap = std::optional<std::map<std::string_view, std::string_view>>;
		struct InsertStrData {
			size_t keySize;
			size_t keyStart;

			size_t valueSize;
			size_t valueStart;
		};
		//DRY!
		void InsertDataFn(const OptionalStrMap& srcData, std::vector<InsertStrData>& data, std::vector<InsertStrData>* LocalFuzzyData = nullptr);
		void CreateViewOnMap(std::map<std::string_view, std::string_view>& Target, const std::vector<InsertStrData>& data);
		static std::string_view GetOnMapData(const OptionalStrViewMap& map, const std::string_view s);
		//不是StringPoolBase的派生类，是用于Keyboard持有字符串生命周期的内存池
		class StrPool {
		public: //作为字符串视图的数据源，他不需要终止符
			size_t put(const std::string_view& str) {
				size_t result = strs.size();
				strs.insert(strs.end(), str.begin(), str.end());
				return result;
			}
			size_t putChar(const char c) {
				size_t result = strs.size();
				strs.push_back(c);
				return result;
			}
			void putCharPtr(const char* str, size_t size) {
				for (size_t i = 0; i < size; i++) {
					strs.push_back(str[i]);
				}
			}
			char* data() {
				return strs.data();
			}
		private:
			std::vector<char> strs;
		};
		StrPool pool;
		OptionalStrViewMap MapLocal;
		OptionalStrViewMap MapLocalFuuzy;
		OptionalStrViewMap MapKeys;
		CutterFn cutter = standard;
	};
}
