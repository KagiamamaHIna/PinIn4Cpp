#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <exception>
#include <unordered_map>
#include <set>

namespace PinInCpp {
	//Unicode码转utf8字节流
	std::string UnicodeToUtf8(char32_t);
	//十六进制数字字符串转int
	int HexStrToInt(const std::string&);

	std::vector<std::string> split(const std::string& str, char delimiter);

	class Utf8String {
	public:
		Utf8String(const std::string& input);

		std::string ToStream()const {
			std::string result;
			for (const auto& v : str) {
				result += v;
			}
			return result;
		}

		std::string& operator[](size_t i) {
			return str[i];
		}

		const std::string& operator[](size_t i)const {
			return str[i];
		}

		std::string& at(size_t i) {
			return str.at(i);
		}

		const std::string& at(size_t i)const {
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
		std::vector<std::string> str;
	};

	class Utf8StringView {
	public:
		Utf8StringView(const std::string_view& input);

		std::string_view& operator[](size_t i) {
			return str[i];
		}

		const std::string_view& operator[](size_t i)const {
			return str[i];
		}

		std::string_view& at(size_t i) {
			return str.at(i);
		}

		const std::string_view& at(size_t i)const {
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
		std::vector<std::string_view> str;
	};

	class PinyinFileNoGet : public std::exception {
	public:
		virtual const char* what() {
			return "File not successfully opened";
		}
	};

	/*class Phoneme {
	public:

	private:
		std::vector<std::string> strs;
	};*/

	static constexpr size_t NullPinyinId = static_cast<size_t>(-1);

	class PinIn {
	public:
		PinIn(const std::string& path);
		size_t GetPinyinId(const std::string& hanzi)const {
			auto it = data.find(hanzi);
			if (it == data.end()) {
				return NullPinyinId;
			}
			return it->second;
		}
		std::vector<std::string> GetPinyinById(const size_t id, bool hasTone)const;//你不应该传入非法的id，可能会造成未定义行为，GetPinyinId返回的都是合法的
		std::vector<std::string_view> GetPinyinViewById(const size_t id, bool hasTone)const;//只读版接口

		std::vector<std::string> GetPinyin(const std::string& str, bool hasTone = false)const;//处理单汉字的拼音
		std::vector<std::string_view> GetPinyinView(const std::string& str, bool hasTone = false)const;//只读版接口

		std::vector<std::vector<std::string>> GetPinyinList(const std::string& str, bool hasTone = false)const;//处理多汉字的拼音
		std::vector<std::vector<std::string_view>> GetPinyinViewList(const std::string& str, bool hasTone = false)const;//只读版接口
		bool empty()const {//返回有效性，真即有效，假即无效
			return pool.empty();
		}

		bool HasPinyin(const std::string& str)const;

		static bool hasInitial(const std::string& s) {//判断是否有声母
			if (s.empty()) {
				return false;
			}
			//检查第一个字符
			switch (s.front()) {
			case 'a':
			case 'e':
			case 'i':
			case 'o':
			case 'u':
			case 'v': //'v' 代表 'ü'
				return false; //如果是元音开头，说明没有（辅音）声母
			default:
				return true;  //其他所有情况（辅音开头），说明有声母
			}
		}

	private:
		//不是StringPoolBase的派生类，是用于Pinyin的内存空间优化的类
		class CharPool {//字符每一个拼音都是唯一的，不需要查重，也不需要删改
		public:
			size_t put(const std::string_view& s);
			size_t putChar(const char s);
			void putEnd();
			std::vector<std::string> getPinyinVec(size_t i)const;
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
		static std::vector<T> DeletaTone(const PinIn* ctx, size_t id) {
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

		bool fZh2Z = false;
		bool fSh2S = false;
		bool fCh2C = false;
		bool fAng2An = false;
		bool fIng2In = false;
		bool fEng2En = false;
		bool fU2V = false;
		bool accelerate = false;

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
}
