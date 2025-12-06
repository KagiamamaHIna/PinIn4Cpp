#include "PinIn4Cpp/PinIn.h"//避免循环依赖问题

#include "PinIn4Cpp/detail/StringPool.h"
#include "PinIn4Cpp/detail/BinUtils.h"

namespace PinInCpp {
	namespace detail {
		size_t UTF8StringPool::put(std::string_view s) {
			size_t result = strs.size();

			size_t cursor = 0;
			size_t end = s.size();
			while (cursor < end) {
				size_t charSize = getUTF8CharSize(s[cursor]);
				strs.emplace_back(FourCCToU32(s.substr(cursor, charSize)));
				cursor += charSize;
			}
			strs.emplace_back(0);
			return result;
		}

		std::string UTF8StringPool::getchar(size_t i)const {
			std::string result;
			char buf[5];
			U32FourCCToCharBuf(buf, strs[i]);
			result.append(buf);
			return result;
		}

		std::string UTF8StringPool::getstr(size_t strStart)const {
			std::string result;
			char buf[5];

			uint32_t fourCC = strs[strStart];
			while (fourCC) {
				U32FourCCToCharBuf(buf, fourCC);
				result.append(buf);
				strStart++;
				fourCC = strs[strStart];
			}
			return result;
		}

		size_t UTF8StringPool::getStrSize(size_t strStart)const {
			size_t result = 0;
			uint32_t c = strs[strStart];
			while (strs[strStart]) {
				if (c <= 0xFF) {
					result += 1;
				}
				else if (c <= 0xFFFF) {
					result += 2;
				}
				else if (c <= 0xFFFFFF) {
					result += 3;
				}
				else {
					result += 4;
				}
				strStart++;
				c = strs[strStart];
			}
			return result;
		}

		int UTF8StringPool::PutToCharBuf(size_t strStart, char* buf, size_t bufSize) {
			if (bufSize == 0) {
				return -1;
			}
			char CharBuf[5];
			size_t cursor = 0;//下一次写入字符的位置

			uint32_t fourCC = strs[strStart];
			while (fourCC) {
				size_t size = U32FourCCToCharBuf(CharBuf, fourCC);
				if (cursor + size >= bufSize) {//检查推进后的游标是否大于等于缓冲区大小，如果大于等于意味着装不下字符+终止符
					buf[cursor] = '\0';
					return -1;
				}
				memcpy(buf + cursor, CharBuf, size);

				cursor += size;
				strStart++;
				fourCC = strs[strStart];
			}
			buf[cursor] = '\0';
			return 0;
		}

		/*std::string_view UTF8StringPool::getchar_view(size_t i)const noexcept {
			size_t end = chars_offset[i + 1];
			size_t start = chars_offset[i];

			return std::string_view(strs.data() + start, end - start);
		}

		std::string_view UTF8StringPool::getstr_view(size_t strStart)const noexcept {
			strStart = chars_offset[strStart];
			size_t i = strStart;
			while (strs[i]) {
				i++;
			}
			return std::string_view(strs.data() + strStart, i - strStart);
		}*/

		UTF8StringPool UTF8StringPool::Deserialize(const std::vector<uint8_t>& data, size_t index) {
			VecU8Reader reader(data, index);
			uint32_t ver = reader.GetDoubleWord();
			if (ver != BinDataVersion) {
				throw BinaryVersionInvalidException("UTF8StringPool: Invalid binary file version");
			}
			UTF8StringPool result;

			size_t strsSize = reader.GetSizeTFromQW();
			result.strs.reserve(strsSize);

			for (size_t i = 0; i < strsSize; i++) {
				result.strs.emplace_back(reader.GetDoubleWord());
			}
			return result;
		}

		std::optional<UTF8StringPool> UTF8StringPool::DeserializeFromFile(std::string_view path, size_t index) {
			std::optional<std::vector<uint8_t>> data = ReadBinFile(std::string(path));
			if (!data.has_value()) {
				return std::nullopt;
			}
			return Deserialize(data.value(), index);
		}

		std::vector<uint8_t> UTF8StringPool::Serialize()const {
			std::vector<uint8_t> result;
			PushDWUint8(result, BinDataVersion);

			PushQWUint8(result, strs.size());
			for (const uint32_t fourCC : strs) {
				PushDWUint8(result, fourCC);
			}
			return result;
		}

		bool UTF8StringPool::SerializeToFile(std::string_view path)const {
			return WriteBinFile(std::string(path), Serialize());
		}
	}
}
