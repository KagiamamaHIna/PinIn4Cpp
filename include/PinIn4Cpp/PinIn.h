#pragma once
#include <fstream>
#include <string>
#include <exception>
#include <unordered_map>
#include <set>
#include <cmath>
#include <cstdint>
#include <memory>
#include <cstring>

#include "Keyboard.h"

#include "PinIn4Cpp/detail/IndexSet.h"
#include "PinIn4Cpp/detail/BinUtils.h"
#include "PinIn4Cpp/detail/StringUtils.h"

namespace PinInCpp {
	constexpr uint32_t BinDataVersion = 4;//二进制数据文件id
	static constexpr size_t NullPinyinId = static_cast<size_t>(-1);//空拼音id

	class PinyinFileNotOpenException : public std::exception {
	public:
		const char* what()const noexcept override {
			return "File not successfully opened";
		}
	};
	class BinaryVersionInvalidException : public std::exception {
	public:
		BinaryVersionInvalidException(const char* str = "Invalid binary file version") :str{ str } {
		}
		const char* what()const noexcept override {
			return str.c_str();
		}
		std::string str;
	};

	//文件解析策略为：跳过错误行
	class PinIn {
	public:
		class Character;//你应该在这里，因为你是公开接口里返回的对象！(向前声明)
		PinIn(std::string_view path);
		PinIn(const std::vector<char>& input_data);//数据加载模式

		//可以用这个进行快捷打包，函数定义就在这个文件末尾:PinInDataPack(std::string_view dataPath, std::string_view binPath)

		//32位环境和64位环境生成的文件格式应该是通用的，但是如果64位下数据量过大，32位环境下加载有潜在的溢出导致逻辑错误的风险
		//你应该只传入对应类的Serialization方法生成的数据，传入一个不正确格式的很有可能会造成未定义行为
		//不过有可能抛出std::out_of_range，抛出这个代表读取时越界了，通常意味着结构错误
		//抛出BinaryVersionInvalidException代表版本号错误，不可使用
		//Keyboard本身是无法序列化的，手动传入你想要的，不传用默认的全拼
		static std::shared_ptr<PinIn> Deserialize(const std::vector<uint8_t>& data, const std::optional<Keyboard>& keyboard = std::nullopt, size_t index = 0);
		static std::optional<std::shared_ptr<PinIn>> DeserializeFromFile(std::string_view path, const std::optional<Keyboard>& keyboard = std::nullopt, size_t index = 0) {
			std::optional<std::vector<uint8_t>> data = detail::ReadBinFile(std::string(path));
			if (!data.has_value()) {
				return std::nullopt;
			}
			return Deserialize(data.value(), keyboard, index);
		}
		std::vector<uint8_t> Serialize()const;
		//返回真代表写入成功
		bool SerializeToFile(std::string_view path)const {
			return detail::WriteBinFile(std::string(path), Serialize());
		}

		std::vector<uint8_t> PreCacheSerialize()const;
		void PreCacheDeserialize(const std::vector<uint8_t>& data, size_t index = 0);

		//返回的是汉字拼音id，不是单拼音的拼音id
		size_t GetPinyinId(const uint32_t hanziFourCC)const {
			auto it = data.find(hanziFourCC);
			return it == data.end() ? NullPinyinId : it->second;
		}
		//返回的是汉字拼音id，不是单拼音的拼音id
		size_t GetPinyinId(std::string_view hanzi)const {
			return GetPinyinId(detail::FourCCToU32(hanzi));
		}
		std::vector<std::string> GetPinyinById(const size_t id, bool hasTone)const;//你不应该传入非法的id，可能会造成未定义行为，GetPinyinId返回的都是合法的
		std::vector<std::string_view> GetPinyinViewById(const size_t id, bool hasTone)const;//只读版接口，视图的数据生命周期跟随PinIn对象

		std::vector<std::string> GetPinyin(std::string_view str, bool hasTone = false)const;//处理单汉字的拼音
		std::vector<std::string_view> GetPinyinView(std::string_view str, bool hasTone = false)const;//只读版接口，视图的数据生命周期跟随PinIn对象

		std::vector<std::vector<std::string>> GetPinyinList(std::string_view str, bool hasTone = false)const;//处理多汉字的拼音
		std::vector<std::vector<std::string_view>> GetPinyinViewList(std::string_view str, bool hasTone = false)const;//只读版接口，视图的数据生命周期跟随PinIn对象

		Character GetChar(std::string_view str) {//会始终构建一个Character，比较浪费性能
			return Character(*this, str, GetPinyinId(str));
		}
		Character GetChar(const uint32_t fourCC) {//同上
			char buf[5];
			detail::U32FourCCToCharBuf(buf, fourCC);
			return Character(*this, buf, GetPinyinId(fourCC));
		}
		Character* GetCharCachePtr(std::string_view str);//缓存关闭时返回空指针，开启时返回有效数据，注意，无效的字符串在缓存存储后再次返回都是第一个访问时的无效的字符串
		Character* GetCharCachePtr(const uint32_t fourCC);//同上

		//字符缓存预热，可以用待选项/搜索字符串预热，避免缓存的多线程数据竞争问题，如果是单线程的则不用管
		void PreCacheString(std::string_view str);
		//强制生成一个空拼音id的缓存，配合上面那个api即可实现线程安全
		void PreNullPinyinIdCache() {
			if (!CharCache || CharCache.value().count(NullPinyinId)) {//如果关闭了缓存或者NullPinyinId有值，则不执行
				return;
			}
			std::unordered_map<size_t, std::unique_ptr<Character>>& cache = CharCache.value();
			cache.insert_or_assign(NullPinyinId, std::unique_ptr<Character>(new Character(*this, "", NullPinyinId)));
		}
		bool IsCharCacheEnabled()const noexcept {
			return CharCache.has_value();
		}
		void SetCharCache(bool enable) {//默认开启缓存
			if (enable && !CharCache.has_value()) {//如果启用且没有值的时候
				CharCache = std::unordered_map<size_t, std::unique_ptr<Character>>();
			}
			else if (!enable) {//未启用的时候清空
				CharCache.reset();
			}
		}

		bool empty()const noexcept {//返回是否为空，真即无效，假即有效
			return pool.empty();
		}
		bool HasPinyin(std::string_view str)const noexcept;

		class Ticket {
		public:
			Ticket(const PinIn& ctx, const std::function<void()>& fn) : runnable{ fn }, ctx{ ctx } {
				modification = ctx.modification;
			}
			void renew() {
				int i = ctx.modification;
				if (modification != i) {
					modification = i;
					runnable();
				}
			}
		private:
			const std::function<void()> runnable;//任务
			const PinIn& ctx;//绑定的拼音上下文
			int modification;
		};
		std::unique_ptr<Ticket> ticket(const std::function<void()>& r)const {//转移所有权，让你能持有这个对象
			return std::make_unique<Ticket>(*this, r);
		}

		const Keyboard& getkeyboard()const noexcept {
			return keyboard;
		}
		bool getfZh2Z()const noexcept {
			return fZh2Z;
		}
		bool getfSh2S()const noexcept {
			return fSh2S;
		}
		bool getfCh2C()const noexcept {
			return fCh2C;
		}
		bool getfAng2An()const noexcept {
			return fAng2An;
		}
		bool getfIng2In()const noexcept {
			return fIng2In;
		}
		bool getfEng2En()const noexcept {
			return fEng2En;
		}
		bool getfU2V()const noexcept {
			return fU2V;
		}
		bool getfFirstChar()const noexcept {
			return fFirstChar;
		}
		class Config {
		public://不提供函数式的链式调用接口了
			Config(PinIn& ctx);
			Keyboard keyboard;
			bool fZh2Z = false;
			bool fSh2S = false;
			bool fCh2C = false;
			bool fAng2An = false;
			bool fIng2In = false;
			bool fEng2En = false;
			bool fU2V = false;
			bool fFirstChar = false;
			//将当前Config对象中的所有设置应用到PinIn上下文中。此方法总会触发数据的更改，无论配置是否实际发生变化，调用者应负责避免不必要的或重复的commit()调用
			//重载完成后，音素这样的数据的视图不再合法，需要重载(重载字符类即可)，可以用Ticket类注册一个异步操作，在每次执行前检查后按需重载(执行Ticket::renew触发回调函数)
			void commit();
		private:
			PinIn& ctx;//绑定的拼音上下文
		};
		Config config() {//修改拼音类配置
			return Config(*this);
		}

		//权责关系:Phoneme->Pinyin->Character->PinIn
		class Element {//基类，确保这些成分都像原始的设计一样，可以被转换为这个基本的类
		public:
			virtual ~Element() = default;
			virtual detail::IndexSet match(const detail::Utf8StringView& source, size_t start, bool partial)const = 0;
			virtual std::string ToString()const = 0;
		};
		class Pinyin;
		class Phoneme : public Element {
		public:
			/*
			你应该在Config.commit后把自己缓存的音素重载了

			我发现模糊匹配这个流程，字符串拼接、查找表等也是能避免的，只需要做简单的字符串/字符比较，理由如下：
			z/s/c的模糊音，非常简单，不过多论述
			v->u的模糊音，遇到j/q/x/y的时候，v不再写为v，而是写为u，
			导致其只有v和ve的操作需要单独列出来，这将数据量压到了一个非常小范围，我们可以做一次字符串长度检查完成是v还是ve的操作
			ang/eng/ing和an/en/in的模糊音，
			实际上和z/s/c的情况类似了，因为不需要前缀拼接，即他们是单独的音素，而不是一个完整的拼音，所以简化成了z/s/c的模糊音规则即可，
			最后把对应的结果用字符串字面量写进去:)

			有Local情况下的reload函数可以用纯逻辑+查表混合模式，
			因为c/s/z作为检查开头的音素，他们本质上还是那么简单，
			ang/ing/eng和去掉g的情况要考虑复杂的映射表，所以也是要查表匹配

			所以！v+后缀字符串和ang/ing/eng这些是真正需要在有Local的情况下查表匹配的，我们可以将这两个的逻辑简单的分离成私有成员方法，就能完美的实现了
			*/
			virtual ~Phoneme() = default;
			virtual std::string ToString()const {
				return std::string(strs[0]);
			}
			bool empty()const noexcept {//没有数据当然就是空了，如果要代表一个空音素，本质上不需要存储任何东西
				return strs.empty();
			}
			bool matchSequence(const char c)const noexcept;
			detail::IndexSet match(const detail::Utf8StringView& source, detail::IndexSet idx, size_t start, bool partial)const noexcept;
			detail::IndexSet match(const detail::Utf8StringView& source, size_t start, bool partial)const noexcept;
			const std::vector<std::string_view>& GetAtoms()const noexcept {//获取这个音素的最小成分(原子)，即它表达了什么音素
				return strs;
			}
			const std::string_view& GetSrc()const noexcept {
				return src;
			}
		private:
			friend Pinyin;//由Pinyin类执行构建
			void reload();//本质上只需要代表好它的对象即可，本质上应该禁用，因为切换时音素本身也有可能会被切换，这时候视图可能是危险的，要确保重载行为在框架内是合理的
			explicit Phoneme(PinIn& ctx, std::string_view src) :ctx{ ctx }, src{ src } {//私有构造函数，因为只读视图之类的原因，用一个编译期检查的设计避免他被不小心构造
				reload();
			}
			void reloadNoMap();//无Local表的纯逻辑处理
			void reloadHasMap();//有Local表的逻辑查表混合处理

			PinIn& ctx;//直接绑定拼音上下文，方便reload
			const std::string_view src;
			std::vector<std::string_view> strs;//真正用于处理的数据
		};
		class Pinyin : public Element {
		public:
			virtual ~Pinyin() = default;
			const std::vector<Phoneme*>& GetPhonemes()const {//只读接口 返回的是观察者指针 如果PinIn重载了/PinIn被销毁了就会变成悬垂指针
				return phonemes;
			}
			virtual std::string ToString()const {
				return std::string(ctx.pool.getPinyinView(id));
			}
			void reload(std::string_view src);
			detail::IndexSet match(const detail::Utf8StringView& str, size_t start, bool partial)const noexcept;
			const size_t id;//原始设计也是不变的，轻量级id设计，无法通过id反向查询
		private:
			friend Character;//由Character类执行构建
			Pinyin(PinIn& p, size_t id, std::string_view src) :id{ id }, ctx{ p } {
				reload(src);
			}
			PinIn& ctx;
			bool duo = false;
			bool sequence = false;
			std::vector<Phoneme*> phonemes;
		};
		class Character : public Element {
		public:
			virtual ~Character() = default;
			virtual std::string ToString()const {
				return ch;
			}
			bool IsPinyinValid()const noexcept {//检查是否拼音有效 替代Dummy类型，如果返回真则有效
				return id != NullPinyinId;
			}
			const std::string& get()const noexcept {
				return ch;
			}
			const std::vector<Pinyin*>& GetPinyins()const noexcept {//返回的是观察者指针 如果PinIn重载了/PinIn被销毁了就会变成悬垂指针
				return pinyin;
			}
			void reload();//reload可重新恢复拼音和音素的有效性
			detail::IndexSet match(const detail::Utf8StringView& str, size_t start, bool partial)const noexcept;
			const size_t id;//代表这个字符的一个主拼音id
		private:
			friend PinIn;//由PinIn类执行构建
			Character(PinIn& p, std::string_view ch, const size_t id) :id{ id }, ctx{ p }, ch{ ch } {
				reload();
			}
			PinIn& ctx;
			const std::string ch;//需要持有一个字符串，因为这个是依赖输入源的，不是拼音数据
			std::vector<Pinyin*> pinyin;//观察者
		};
	private:
		PinIn() = default;//私有的，用于给反序列化接口生成一个空的PinIn对象

		void LineParser(const detail::Utf8StringView&);
		//不是StringPoolBase的派生类，是用于Pinyin的内存空间优化的类
		class CharPool {//字符每一个拼音都是唯一的，不需要查重，也不需要删改
		public:
			CharPool() {
				strs = std::make_unique<std::vector<char>>();
			}
			CharPool(std::unique_ptr<char[]> data, size_t DataSize) {
				FixedStrs = std::move(data);
				poolSize = DataSize;
			}
			size_t put(std::string_view s) {
				size_t result = strs->size();
				strs->insert(strs->end(), s.begin(), s.end());//插入字符串
				return result;
			}
			size_t putChar(const char s) {
				size_t result = strs->size();
				strs->push_back(s);
				return result;
			}
			void putEnd() {
				strs->push_back('\0');
			}
			std::vector<std::string> getPinyinVec(size_t i)const;
			std::string_view getPinyinView(size_t i)const;
			std::vector<std::string_view> getPinyinViewVec(size_t i, bool hasTone = false)const;//去除声调不去重，去重由公开接口自己去
			bool empty()const noexcept {
				return strs == nullptr ? poolSize == 0 : strs->empty();
			}
			void Fixed() {//构造完成后固定，将原有向量析构掉，用更轻量的std::unique_ptr<char[]>取代，向量预分配开销去除
				FixedStrs = std::unique_ptr<char[]>(new char[strs->size()]);
				memcpy(FixedStrs.get(), strs->data(), strs->size());
				poolSize = strs->size();
				strs.reset(nullptr);
			}
			char* data() noexcept {
				return FixedStrs.get();
			}
			const char* data()const noexcept {
				return FixedStrs.get();
			}
			size_t size()const noexcept {
				return poolSize;
			}
		private:
			std::unique_ptr<std::vector<char>> strs = nullptr;//用这个存储包括向量的结构，优化内存占用的同时存储完整的拼音字符串并提供id
			std::unique_ptr<char[]> FixedStrs = nullptr;
			size_t poolSize = 0;
		};
		CharPool pool;
		std::unordered_map<uint32_t, size_t> data;//用数字size_t是指代内部拼音数字id，可以用pool提供的方法提供向量，用uint32_t代表utf8编码的字符，开销更小，无堆分配
		size_t PinyinTotals = 0;
		std::unordered_map<std::string_view, std::unique_ptr<Pinyin>> pinyins;//数据唯一性缓存
		std::unordered_map<std::string_view, std::unique_ptr<Phoneme>> phonemes;//数据唯一性缓存
		std::optional<std::unordered_map<size_t, std::unique_ptr<Character>>> CharCache = std::unordered_map<size_t, std::unique_ptr<Character>>();//默认开启

		template<typename T>//不需要音调需要处理
		static std::vector<T> DeleteTone(const PinIn* ctx, size_t id) {
			std::vector<T> result;
			std::set<std::string_view> HasResult;//创建结果集，排除重复选项
			for (const auto& str : ctx->pool.getPinyinViewVec(id, false)) {//直接遍历容器，把有需要的取出来即可，只读的字符串，不涉及拷贝，所需的才会拷贝
				if (!HasResult.count(str)) {
					result.emplace_back(T(str));//深拷贝
					HasResult.insert(str);
				}
			}
			return result;
		}

		Keyboard keyboard = Keyboard::QUANPIN;
		int modification = 0;
		bool fZh2Z = false;
		bool fSh2S = false;
		bool fCh2C = false;
		bool fAng2An = false;
		bool fIng2In = false;
		bool fEng2En = false;
		bool fU2V = false;
		bool fFirstChar = false;//开启首字母匹配，实现更加混合模式的输入(

		struct ToneData {
			char c;
			uint8_t tone;
		};
		//有声调拼音转无声调拼音关联表
		inline static const std::unordered_map<std::string_view, ToneData> toneMap = std::unordered_map<std::string_view, ToneData>({
		{"ā", {'a', 1}}, {"á", {'a', 2}}, {"ǎ", {'a', 3}}, {"à", {'a', 4}},
		{"ē", {'e', 1}}, {"é", {'e', 2}}, {"ě", {'e', 3}}, {"è", {'e', 4}},
		{"ī", {'i', 1}}, {"í", {'i', 2}}, {"ǐ", {'i', 3}}, {"ì", {'i', 4}},
		{"ō", {'o', 1}}, {"ó", {'o', 2}}, {"ǒ", {'o', 3}}, {"ò", {'o', 4}},
		{"ū", {'u', 1}}, {"ú", {'u', 2}}, {"ǔ", {'u', 3}}, {"ù", {'u', 4}},
		{"ü", {'v', 1}}, {"ǘ", {'v', 2}}, {"ǚ", {'v', 3}}, {"ǜ", {'v', 4}},
		{"ń", {'n', 2}}, {"ň", {'n', 3}}, {"ǹ", {'n', 4}},
		{"ḿ", {'m', 2}}, {"m̀", {'m', 4}}
		});
	};
	//快捷打包函数，指定字典文件（data）和输出二进制文件的路径即可
	inline bool PinInDataPack(std::string_view dataPath, std::string_view binPath) {
		return PinIn(dataPath).SerializeToFile(binPath);
	}
}

