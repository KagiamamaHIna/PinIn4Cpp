#include "PinyinFormat.h"

namespace PinInCpp {
	static inline const std::set<std::string> OFFSET = {
		"ui", "iu", "uan", "uang", "ian", "iang", "ua",
		"ie", "uo", "iong", "iao", "ve", "ia"
	};

	static inline const std::map<char, std::string> NONE = {
		{ 'a', "a" }, { 'o', "o" }, { 'e', "e" }, { 'i', "i" }, { 'u', "u" }, { 'v', "ü" }
	};

	static inline const std::map<char, std::string> FIRST = {
		{ 'a', "ā" }, { 'o', "ō" }, { 'e', "ē" }, { 'i', "ī" }, { 'u', "ū" }, { 'v', "ǖ" }
	};

	static inline const std::map<char, std::string> SECOND = {
		{ 'a', "á" }, { 'o', "ó" }, { 'e', "é" }, { 'i', "í" }, { 'u', "ú" }, { 'v', "ǘ" }
	};

	static inline const std::map<char, std::string> THIRD = {
		{ 'a', "ǎ" }, { 'o', "ǒ" }, { 'e', "ě" }, { 'i', "ǐ" }, { 'u', "ǔ" }, { 'v', "ǚ" }
	};

	static inline const std::map<char, std::string> FOURTH = {
		{ 'a', "à" }, { 'o', "ò" }, { 'e', "è" }, { 'i', "ì" }, { 'u', "ù" }, { 'v', "ǜ" }
	};

	static inline const std::vector<std::map<char, std::string>> TONES = { NONE, FIRST, SECOND, THIRD, FOURTH };

	static inline const std::map<std::string, std::string> SYMBOLS = {
			{"a", "ㄚ"}, {"o", "ㄛ"}, {"e", "ㄜ"}, {"er", "ㄦ"}, {"ai", "ㄞ"},
			{"ei", "ㄟ"}, {"ao", "ㄠ"}, {"ou", "ㄡ"}, {"an", "ㄢ"}, {"en", "ㄣ"},
			{"ang", "ㄤ"}, {"eng", "ㄥ"}, {"ong", "ㄨㄥ"}, {"i", "ㄧ"}, {"ia", "ㄧㄚ"},
			{"iao", "ㄧㄠ"}, {"ie", "ㄧㄝ"}, {"iu", "ㄧㄡ"}, {"ian", "ㄧㄢ"}, {"in", "ㄧㄣ"},
			{"iang", "ㄧㄤ"}, {"ing", "ㄧㄥ"}, {"iong", "ㄩㄥ"}, {"u", "ㄨ"}, {"ua", "ㄨㄚ"},
			{"uo", "ㄨㄛ"}, {"uai", "ㄨㄞ"}, {"ui", "ㄨㄟ"}, {"uan", "ㄨㄢ"}, {"un", "ㄨㄣ"},
			{"uang", "ㄨㄤ"}, {"ueng", "ㄨㄥ"}, {"uen", "ㄩㄣ"}, {"v", "ㄩ"}, {"ve", "ㄩㄝ"},
			{"van", "ㄩㄢ"}, {"vang", "ㄩㄤ"}, {"vn", "ㄩㄣ"}, {"b", "ㄅ"}, {"p", "ㄆ"},
			{"m", "ㄇ"}, {"f", "ㄈ"}, {"d", "ㄉ"}, {"t", "ㄊ"}, {"n", "ㄋ"},
			{"l", "ㄌ"}, {"g", "ㄍ"}, {"k", "ㄎ"}, {"h", "ㄏ"}, {"j", "ㄐ"},
			{"q", "ㄑ"}, {"x", "ㄒ"}, {"zh", "ㄓ"}, {"ch", "ㄔ"}, {"sh", "ㄕ"},
			{"r", "ㄖ"}, {"z", "ㄗ"}, {"c", "ㄘ"}, {"s", "ㄙ"}, {"w", "ㄨ"},
			{"y", "ㄧ"}, {"1", ""}, {"2", "ˊ"}, {"3", "ˇ"}, {"4", "ˋ"},
			{"0", "˙"}, {"", ""}
	};

	static inline const std::map<std::string, std::string> LOCAL = {
			{"yi", "i"}, {"you", "iu"}, {"yin", "in"}, {"ye", "ie"}, {"ying", "ing"},
			{"wu", "u"}, {"wen", "un"}, {"yu", "v"}, {"yue", "ve"}, {"yuan", "van"},
			{"yun", "vn"}, {"ju", "jv"}, {"jue", "jve"}, {"juan", "jvan"}, {"jun", "jvn"},
			{"qu", "qv"}, {"que", "qve"}, {"quan", "qvan"}, {"qun", "qvn"}, {"xu", "xv"},
			{"xue", "xve"}, {"xuan", "xvan"}, {"xun", "xvn"}, {"shi", "sh"}, {"si", "s"},
			{"chi", "ch"}, {"ci", "c"}, {"zhi", "zh"}, {"zi", "z"}, {"ri", "r"}
	};

	std::string PinyinFormat(const PinIn::Pinyin& p, PinyinFormatEnum FormatType) {
		std::string result = p.ToString();
		switch (FormatType) {
		case PinyinFormatEnum::FORMAT_PHONETIC: {
			std::string temp = result;
			auto it = LOCAL.find(result.substr(0, result.size() - 1));
			if (it != LOCAL.end()) {
				temp = it->second + temp[temp.size() - 1];
			}
			std::vector<std::string> split;
			size_t len = temp.size();
			if (!hasInitial(temp)) {
				split = { "", temp.substr(0, len - 1), temp.substr(len - 1) };
			}
			else {
				size_t i = temp.size() > 2 && temp[1] == 'h' ? 2 : 1;
				split = { temp.substr(0, i), temp.substr(i, len - i - 1), temp.substr(len - 1) };
			}
			result = "";
			bool weak = split[2][0] == '0';
			if (weak) {
				const std::string& t = SYMBOLS.at(split[2]);
				result.insert(result.end(), t.begin(), t.end());
			}
			const std::string& t1 = SYMBOLS.at(split[0]);
			result.insert(result.end(), t1.begin(), t1.end());
			const std::string& t2 = SYMBOLS.at(split[1]);
			result.insert(result.end(), t2.begin(), t2.end());

			if (!weak) {
				const std::string& t3 = SYMBOLS.at(split[2]);
				result.insert(result.end(), t3.begin(), t3.end());
			}
			break;
		}
		case PinyinFormatEnum::FORMAT_UNICODE: {
			std::string finale;
			std::string temp;
			size_t len = result.size();
			if (!hasInitial(result)) {
				finale = result.substr(0, len - 1);
			}
			else {
				size_t i = result.size() > 2 && result[1] == 'h' ? 2 : 1;
				temp.insert(temp.end(), result.begin(), result.begin() + i);
				finale = result.substr(i, len - 1);
			}
			size_t offset = OFFSET.contains(finale.substr(0, finale.size() - 1)) ? 1 : 0;
			if (offset == 1) {
				temp.insert(temp.end(), finale.begin(), finale.begin() + 1);
			}
			const std::map<char, std::string>& group = TONES[result[result.size() - 1] - '0'];
			const std::string& str = group.at(finale[offset]);
			temp.insert(temp.end(), str.begin(), str.end());
			if (finale.size() > offset + 1) {
				temp.insert(temp.end(), finale.begin() + offset + 1, finale.end() - 1);
			}
			result = temp;
			break;
		}
		case PinyinFormatEnum::FORMAT_RAW: {
			result = result.substr(0, result.size() - 1);
			break;
		}
		case PinyinFormatEnum::FORMAT_NUMBER: {
			break;
		}
		}
		return result;
	}
}
