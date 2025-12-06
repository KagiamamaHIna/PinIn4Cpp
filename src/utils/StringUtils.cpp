#include "PinIn4Cpp/detail/StringUtils.h"

namespace PinInCpp {
	namespace detail {
		uint32_t UnicodeToUtf8(char32_t unicodeChar) noexcept {
			uint32_t utf8 = 0;
			if (unicodeChar <= 0x7F) {
				//1字节数据
				utf8 = static_cast<char>(unicodeChar);
			}
			else if (unicodeChar <= 0x7FF) {
				//2字节数据
				utf8 |= static_cast<uint8_t>(0xC0 | ((unicodeChar >> 6) & 0x1F));
				utf8 <<= 8;
				utf8 |= static_cast<uint8_t>(0x80 | (unicodeChar & 0x3F));
			}
			else if (unicodeChar <= 0xFFFF) {
				//3字节数据
				utf8 |= static_cast<uint8_t>(0xE0 | ((unicodeChar >> 12) & 0x0F));
				utf8 <<= 8;
				utf8 |= static_cast<uint8_t>(0x80 | ((unicodeChar >> 6) & 0x3F));
				utf8 <<= 8;
				utf8 |= static_cast<uint8_t>(0x80 | (unicodeChar & 0x3F));
			}
			else if (unicodeChar <= 0x10FFFF) {
				//4字节数据
				utf8 |= static_cast<uint8_t>(0xF0 | ((unicodeChar >> 18) & 0x07));
				utf8 <<= 8;
				utf8 |= static_cast<uint8_t>(0x80 | ((unicodeChar >> 12) & 0x3F));
				utf8 <<= 8;
				utf8 |= static_cast<uint8_t>(0x80 | ((unicodeChar >> 6) & 0x3F));
				utf8 <<= 8;
				utf8 |= static_cast<uint8_t>(0x80 | (unicodeChar & 0x3F));
			}
			return utf8;
		}

		int HexStrToInt(const std::string& str) {
			try {
				return std::stoi(str, nullptr, 16);
			}
			catch (std::invalid_argument&) {//跳过错误行策略
				return -1;
			}
			catch (std::out_of_range&) {//跳过错误行策略
				return -1;
			}
		}

		uint32_t FourCCToU32(std::string_view str) noexcept {
			uint32_t result = 0;

			size_t size = str.size();
			size = size > 4 ? 4 : size;//限制宽度
			for (size_t i = 0; i < size; i++) {
				result <<= 8;
				result |= (uint8_t)str[i];
			}
			return result;
		}

		size_t U32FourCCToCharBuf(char buf[5], uint32_t c)noexcept {
			size_t size;
			if (c <= 0xFF) {
				size = 1;
			}
			else if (c <= 0xFFFF) {
				size = 2;
			}
			else if (c <= 0xFFFFFF) {
				size = 3;
			}
			else {
				size = 4;
			}
			for (size_t i = 0; i < 5; i++) {
				buf[i] = 0;//缓冲区清空
			}
			for (size_t i = size - 1; i != 0; i--) {
				buf[i] |= c;
				c >>= 8;
			}
			buf[0] |= c;
			return size;
		}
	}
}
