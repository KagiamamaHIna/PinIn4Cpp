#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <exception>
#include <unordered_map>
#include <set>
#include <cmath>

#include "Keyboard.h"
#include "IndexSet.h"

namespace PinInCpp {

	//Unicode码转utf8字节流
	std::string UnicodeToUtf8(char32_t);
	//十六进制数字字符串转int
	int HexStrToInt(const std::string&);

	template<typename StrType>
	class UTF8StringTemplate {
	public:
		UTF8StringTemplate() {}
		UTF8StringTemplate(const StrType& input) {
			size_t cursor = 0;
			size_t end = input.size();
			while (cursor < end) {
				size_t charSize = getUTF8CharSize(input[cursor]);
				str.push_back(input.substr(cursor, charSize));
				cursor += charSize;
			}
		}
		std::string ToStream()const {
			std::string result;
			for (const auto& v : str) {
				result += v;
			}
			return result;
		}
		StrType& operator[](size_t i) {
			return str[i];
		}
		const StrType& operator[](size_t i)const {
			return str[i];
		}
		StrType& at(size_t i) {
			return str.at(i);
		}
		const StrType& at(size_t i)const {
			return str.at(i);
		}
		size_t size()const {
			return str.size();
		}
		auto begin() {
			return str.begin();
		}
		auto end() {
			return str.end();
		}
		const auto begin()const {
			return str.begin();
		}
		const auto end()const {
			return str.end();
		}
	private:
		static size_t getUTF8CharSize(char c) {
			if ((c & 0x80) == 0) { // 0xxxxxxx
				return 1;
			}
			else if ((c & 0xE0) == 0xC0) { // 110xxxxx
				return 2;
			}
			else if ((c & 0xF0) == 0xE0) { // 1110xxxx
				return 3;
			}
			else if ((c & 0xF8) == 0xF0) { // 11110xxx
				return 4;
			}
			else {//这是一个非法的UTF-8首字节
				return 1; //作为错误恢复，把它当作一个单字节处理
			}
		}
		std::vector<StrType> str = {};
	};
	using Utf8String = UTF8StringTemplate<std::string>;
	using Utf8StringView = UTF8StringTemplate<std::string_view>;

	class PinyinFileNoGet : public std::exception {
	public:
		virtual const char* what() {
			return "File not successfully opened";
		}
	};
	static constexpr size_t NullPinyinId = static_cast<size_t>(-1);

	//应当设计一个拼音类，他存储着音素类，通过将拼音字符串重解析，生成一套音素

	class PinIn {
	public:
		class Character;//你应该在这里，因为你是公开接口里返回的对象！(向前声明)
		PinIn(const std::string& path);
		size_t GetPinyinId(const std::string& hanzi)const {
			auto it = data.find(hanzi);
			return it == data.end() ? NullPinyinId : it->second;
		}
		std::vector<std::string> GetPinyinById(const size_t id, bool hasTone)const;//你不应该传入非法的id，可能会造成未定义行为，GetPinyinId返回的都是合法的
		std::vector<std::string_view> GetPinyinViewById(const size_t id, bool hasTone)const;//只读版接口

		std::vector<std::string> GetPinyin(const std::string& str, bool hasTone = false)const;//处理单汉字的拼音
		std::vector<std::string_view> GetPinyinView(const std::string& str, bool hasTone = false)const;//只读版接口

		std::vector<std::vector<std::string>> GetPinyinList(const std::string& str, bool hasTone = false)const;//处理多汉字的拼音
		std::vector<std::vector<std::string_view>> GetPinyinViewList(const std::string& str, bool hasTone = false)const;//只读版接口

		Character GetChar(const std::string& str)const {
			return Character(*this, str);
		}

		bool empty()const {//返回有效性，真即有效，假即无效
			return pool.empty();
		}
		bool HasPinyin(const std::string& str)const;

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
			const PinIn& ctx;//绑定的拼音上下文
			const std::function<void()> runnable;//任务
			int modification;
		};
		std::unique_ptr<Ticket> ticket(const std::function<void()>& r)const {//转移所有权，让你能持有这个对象
			return std::make_unique<Ticket>(*this, r);
		}

		const Keyboard& getkeyboard()const {
			return keyboard;
		}
		bool getfZh2Z()const {
			return fZh2Z;
		}
		bool getfSh2S()const {
			return fSh2S;
		}
		bool getfCh2C()const {
			return fCh2C;
		}
		bool getfAng2An()const {
			return fAng2An;
		}
		bool getfIng2In()const {
			return fIng2In;
		}
		bool getfEng2En()const {
			return fEng2En;
		}
		bool getfU2V()const {
			return fU2V;
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
			//将当前Config对象中的所有设置应用到PinIn上下文中。此方法总会触发数据的更改，无论配置是否实际发生变化，调用者应负责避免不必要的或重复的commit()调用
			//重载完成后，音素这样的数据的视图不再合法，需要重载，可以用Ticket类注册一个异步操作，在每次执行前检查后按需重载(执行Ticket::renew触发回调函数)
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
			virtual IndexSet match(const Utf8String& source, size_t start, bool partial)const = 0;
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
			ang/ing/eng和去掉g的情况也类似，因为他们本质上还是经过了cutter的处理，
			会被区分后分割出来，
			所以！只有v+后缀字符串是真正需要在有Local的情况下查表匹配的，我们可以将这两个的逻辑简单的分离成私有成员方法，就能完美的实现了
			*/
			virtual ~Phoneme() = default;
			virtual std::string ToString()const {
				return std::string(strs[0]);
			}
			void reload();//本质上只需要代表好它的对象即可
			bool empty()const {//没有数据当然就是空了，如果要代表一个空音素，本质上不需要存储任何东西
				return strs.empty();
			}
			bool matchSequence(const Utf8String& c)const;
			IndexSet match(const Utf8String& source, IndexSet idx, size_t start, bool partial)const;
			IndexSet match(const Utf8String& source, size_t start, bool partial)const;
			const std::vector<std::string_view>& GetAtoms()const {//获取这个音素的最小成分(原子)，即它表达了什么音素
				return strs;
			}
			const std::string_view& GetSrc()const {
				return src;
			}
		private:
			friend Pinyin;//由Pinyin类执行构建
			explicit Phoneme(const PinIn& ctx, std::string_view src) :ctx{ ctx }, src{ src } {//私有构造函数，因为只读视图之类的原因，用一个编译期检查的设计避免他被不小心构造
				reload();
			}
			void reloadNoMap();//无Local表的纯逻辑处理
			void reloadHasMap();//有Local表的逻辑查表混合处理

			const PinIn& ctx;//直接绑定拼音上下文，方便reload
			const std::string_view src;
			std::vector<std::string_view> strs;//真正用于处理的数据
		};
		class Pinyin : public Element {
		public:
			virtual ~Pinyin() = default;
			const std::vector<Phoneme>& GetPhonemes()const {//只读接口
				return phonemes;
			}
			virtual std::string ToString()const {
				return std::string(ctx.pool.getPinyinView(id));
			}
			void reload();
			IndexSet match(const Utf8String& str, size_t start, bool partial)const;
			const size_t id;//原始设计也是不变的，轻量级id设计，可用此id直接重载数据，不直接持有拼音字符串视图
		private:
			friend Character;//由Character类执行构建
			Pinyin(const PinIn& p, size_t id) :ctx{ p }, id{ id } {
				reload();
			}
			const PinIn& ctx;
			bool duo = false;
			bool sequence = false;
			std::vector<Phoneme> phonemes;
		};
		class Character : public Element {
		public:
			virtual ~Character() = default;
			virtual std::string ToString()const {
				return ch;
			}
			bool IsPinyinValid()const {//检查是否拼音有效 替代Dummy类型
				return id == NullPinyinId;
			}
			const std::string& get()const {
				return ch;
			}
			const std::vector<Pinyin>& GetPinyins()const {
				return pinyin;
			}
			IndexSet match(const Utf8String& str, size_t start, bool partial)const;
			const size_t id;//代表这个字符的一个主拼音id
		private:
			friend PinIn;//由PinIn类执行构建
			Character(const PinIn& p, const std::string& ch);
			const PinIn& ctx;
			const std::string ch;//需要持有一个字符串，因为这个是依赖输入源的，不是拼音数据
			std::vector<Pinyin> pinyin;
		};
	private:
		//不是StringPoolBase的派生类，是用于Pinyin的内存空间优化的类
		class CharPool {//字符每一个拼音都是唯一的，不需要查重，也不需要删改
		public:
			size_t put(const std::string_view& s);
			size_t putChar(const char s);
			void putEnd();
			std::vector<std::string> getPinyinVec(size_t i)const;
			std::string_view getPinyinView(size_t i)const;
			std::vector<std::string_view> getPinyinViewVec(size_t i, bool hasTone = false)const;//去除声调不去重，去重由公开接口自己去
			bool empty()const {
				return strs.empty();
			}
		private:
			std::vector<char> strs;//用这个存储包括向量的结构，优化内存占用的同时存储完整的拼音字符串并提供id
		};
		CharPool pool;
		std::unordered_map<std::string, size_t> data;//用数字size_t是指代内部拼音数字id，可以用pool提供的方法提供向量

		template<typename T>//不需要音调需要处理
		static std::vector<T> DeleteTone(const PinIn* ctx, size_t id) {
			std::vector<T> result;
			std::set<std::string_view> HasResult;//创建结果集，排除重复选项
			for (const auto& str : ctx->pool.getPinyinViewVec(id, false)) {//直接遍历容器，把有需要的取出来即可，只读的字符串，不涉及拷贝，所需的才会拷贝
				if (!HasResult.count(str)) {
					result.push_back(T(str));//深拷贝
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
	bool operator==(const PinIn::Phoneme& lhs, const PinIn::Phoneme& rhs);
}
namespace std {
	template <>//特化一个这样的类
	struct hash<PinInCpp::PinIn::Phoneme> {
		std::size_t operator()(const PinInCpp::PinIn::Phoneme& p) const {
			return std::hash<std::string_view>{}(p.GetSrc());
		}
	};
}
