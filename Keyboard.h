#pragma once
#include <map>
#include <string>
#include <optional>
#include <functional>

#include "PinIn.h"

/*
	拼音上下文是带声调的！
	拼音字符应当都是ASCII可表示的字符，不然字符串处理会出问题
*/

namespace PinInCpp {
	using OptionalStrMap = std::optional<std::map<std::string, std::string>>;
	using CutterFn = std::function<std::vector<std::string>(const std::string&)>;

	class Keyboard {
	public:
		Keyboard(const OptionalStrMap& MapLocal, const OptionalStrMap& MapKeys, CutterFn cutter, bool duo, bool sequence)
			:MapLocal{ MapLocal }, MapKeys{ MapKeys }, cutter{ cutter }, duo{ duo }, sequence{ sequence } {
		}
		virtual ~Keyboard() = default;

		std::string keys(const std::string& s)const;
		std::vector<std::string> split(const std::string& s)const;

		static std::vector<std::string> standard(const std::string& s);
		static std::vector<std::string> zero(const std::string& s);

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
		OptionalStrMap MapLocal;
		OptionalStrMap MapKeys;
		CutterFn cutter;
	};
}
