#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>

namespace PinInCpp {
	namespace detail {
		//Unicode码转utf8字符
		uint32_t UnicodeToUtf8(char32_t)noexcept;
		//十六进制数字字符串转int
		int HexStrToInt(const std::string&);
		//将字符串转换为uint32数字表示（只转换前四个）
		uint32_t FourCCToU32(std::string_view str) noexcept;
		//提供一个缓冲区，在缓冲区里面构建回单字符的字节流，返回当前字符大小(不包括终止符)
		size_t U32FourCCToCharBuf(char buf[5], uint32_t c) noexcept;
		inline size_t getUTF8CharSize(const char c) noexcept {
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
		template<typename StrType>
		class UTF8StringTemplate {
		public:
			UTF8StringTemplate() {}
			UTF8StringTemplate(std::string_view input) {
				Init(input);
			}
			void reset(std::string_view input) {
				str.clear();
				Init(input);
			}
			void ShrinkToFit() {
				str.shrink_to_fit();
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
			size_t size()const noexcept {
				return str.size();
			}
			auto begin() noexcept {
				return str.begin();
			}
			auto end() noexcept {
				return str.end();
			}
			const auto begin()const noexcept {
				return str.begin();
			}
			const auto end()const noexcept {
				return str.end();
			}
		private:
			void Init(std::string_view input) {
				size_t cursor = 0;
				size_t end = input.size();
				while (cursor < end) {
					size_t charSize = getUTF8CharSize(input[cursor]);
					str.emplace_back(input.substr(cursor, charSize));
					cursor += charSize;
				}
			}
			std::vector<StrType> str;
		};
		using Utf8String = UTF8StringTemplate<std::string>;
		using Utf8StringView = UTF8StringTemplate<std::string_view>;
	}
}
