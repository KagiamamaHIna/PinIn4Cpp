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
	namespace detail {
		bool hasInitial(std::string_view s);//本质上就是一个处理函数，直接放这里也没啥
	}
	using OptionalStrMap = std::optional<std::map<std::string, std::string>>;

	class Keyboard;
	//应该只处理音节，无需处理音调，自定义逻辑的时候请注意！和原始Java项目行为不一致
	//你不应该在里面手动进行音素的改造，比如v->u，他们应该是交由其他的方法处理的，这是纯粹的切割音素函数
	using CutterFn = std::function<std::vector<std::string_view>(std::string_view)>;

	class Keyboard {
	public:
		//duo参数为真代表这是个双拼方案，sequence为真代表启用序列匹配，只匹配单ASCII字符(一般用于全拼)
		Keyboard(const OptionalStrMap& MapLocalArg, const OptionalStrMap& MapKeysArg, CutterFn cutter, bool duo, bool sequence);

		Keyboard(const Keyboard& src);
		Keyboard& operator=(const Keyboard& src);
		//移动构造函数应该是安全的，因为向量也会被移动
		Keyboard(Keyboard&&) noexcept = default;
		Keyboard& operator=(Keyboard&&) noexcept = default;

		~Keyboard() = default;

		std::string_view keys(std::string_view s)const noexcept;
		std::vector<std::string_view> GetFuzzyPhoneme(std::string_view s)const;
		std::vector<std::string_view> split(std::string_view s)const;
		bool GetHasFuuzyLocal()const noexcept {//用于确定音素reload是否进行查表和纯逻辑行为
			return MapLocalFuzzy.has_value();
		}

		//本身就是一个标准的，处理全拼音素的CutterFn
		static std::vector<std::string_view> Standard(std::string_view s);

		//在standard的基础上，零声母情况下的 韵母第一个字母+韵母所在键 方案的CutterFn
		static std::vector<std::string_view> Zero(std::string_view s);

		//在standard的基础上，零声母情况下的 韵母第一个字母+韵母所在键，两字母的韵母全拼 方案的CutterFn，给自然码/小鹤双拼双拼使用的
		static std::vector<std::string_view> ZeroZiranmaOrXiaohe(std::string_view s);

		//在standard的基础上，零声母情况下的声母为 o 方案CutterFn
		static std::vector<std::string_view> ZeroOInitial(std::string_view s);

		//在standard的基础上，零声母情况下的声母为 a 方案CutterFn
		static std::vector<std::string_view> ZeroAInitial(std::string_view s);

		static const Keyboard QUANPIN;//基础的全拼方案 PinIn类的默认方案
		static const Keyboard DAQIAN;//注音（大千）输入法方案
		static const Keyboard XIAOHE;//小鹤双拼方案
		static const Keyboard ZIRANMA;//自然码双拼方案
		static const Keyboard SOUGOU;//搜狗双拼方案
		static const Keyboard ZHINENG_ABC;//智能ABC双拼方案
		static const Keyboard GUOBIAO;//国标双拼方案
		static const Keyboard MICROSOFT;//微软双拼方案
		static const Keyboard PINYINPP;//拼音加加双拼方案
		static const Keyboard ZIGUANG;//紫光双拼方案

		bool duo;
		bool sequence;
	private:
		using OptionalStrViewMap = std::optional<std::map<std::string_view, std::string_view>>;
		using OptionalStrViewVecMap = std::optional<std::map<std::string_view, std::vector<std::string_view>>>;
		struct InsertStrData {
			size_t keySize;
			size_t keyStart;

			size_t valueSize;
			size_t valueStart;
		};
		struct InsertStrMultiData {
			size_t keySize;//一键对多个值
			size_t keyStart;
			struct value {
				size_t valueSize;
				size_t valueStart;
			};
			std::vector<value> values;
		};
		//DRY!
		void InsertDataFn(const OptionalStrMap& srcData, std::vector<InsertStrData>& data, std::vector<InsertStrMultiData>* LocalFuzzyData = nullptr);
		void CreateViewOnMap(std::map<std::string_view, std::string_view>& Target, const std::vector<InsertStrData>& data);
		void copy(const Keyboard& src);
		class StrPool;
		void ViewDeepCopy(const std::map<std::string_view, std::string_view>& srcMap, std::map<std::string_view, std::string_view>& Target);

		//不是StringPoolBase的派生类，是用于Keyboard持有字符串生命周期的内存池
		class StrPool {
		public: //作为字符串视图的数据源，他不需要终止符
			StrPool() = default;
			StrPool(const StrPool&) = default;
			size_t put(std::string_view str) {
				size_t result = strs.size();
				strs.insert(strs.end(), str.begin(), str.end());
				return result;
			}
			size_t putChar(const char c) {
				size_t result = strs.size();
				strs.emplace_back(c);
				return result;
			}
			void putCharPtr(const char* str, size_t size) {
				for (size_t i = 0; i < size; i++) {
					strs.emplace_back(str[i]);
				}
			}
			char* data() {
				return strs.data();
			}
			const char* data()const {
				return strs.data();
			}
			void reserve(size_t size) {
				strs.reserve(size);
			}
			size_t size()const noexcept {
				return strs.size();
			}
		private:
			std::vector<char> strs;
		};
		StrPool pool;
		OptionalStrViewVecMap MapLocalFuzzy;
		OptionalStrViewMap MapLocal;
		OptionalStrViewMap MapKeys;
		CutterFn cutter;
	};
}
