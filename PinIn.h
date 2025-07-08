#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <exception>
#include <unordered_map>
#include <set>

#include "StringPool.h"

namespace PinInCpp {
	//不是StringPoolBase的派生类，是用于Pinyin的内存空间优化的类
	class CharPool {
	public:
		size_t put(const std::string& s);
		std::string get(size_t i)const;
	private:
		std::vector<char> strs;
	};

	//Unicode码转utf8字节流
	std::string UnicodeToUtf8(char32_t);
	//十六进制数字字符串转int
	int HexStrToInt(const std::string&);

	std::vector<std::string> split(const std::string& str, char delimiter);
	class CharPool;
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

	private:
		std::vector<std::string> str;
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

	class PinIn {
	public:
		PinIn(const std::string& path);
		std::vector<std::string> GetPinyin(const std::string& str, bool hasTone = false)const;//处理单汉字的拼音
		std::vector<std::vector<std::string>> GetPinyinList(const std::string& str, bool hasTone = false)const;//处理多汉字的拼音
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
		std::unordered_map<std::string, std::vector<std::string>> data;//用数字(int32_t)id代表内部拼音方便比较
		//CharPool pool;

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
		inline static const std::unordered_map<std::string, ToneData> toneMap = std::unordered_map<std::string, ToneData>({
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
