#include "PinIn4Cpp/detail/BinUtils.h"

namespace PinInCpp {
	namespace detail {
		uint32_t GetU8VecDW(const std::vector<uint8_t>& srcData, size_t index) {
			uint32_t result = 0;
			result |= srcData[index + 3];
			result <<= 8;
			result |= srcData[index + 2];
			result <<= 8;
			result |= srcData[index + 1];
			result <<= 8;
			result |= srcData[index];
			return result;
		}

		uint64_t GetU8VecQW(const std::vector<uint8_t>& srcData, size_t index) {
			uint64_t result = 0;
			result |= srcData[index + 7];
			result <<= 8;
			result |= srcData[index + 6];
			result <<= 8;
			result |= srcData[index + 5];
			result <<= 8;
			result |= srcData[index + 4];
			result <<= 8;
			result |= srcData[index + 3];
			result <<= 8;
			result |= srcData[index + 2];
			result <<= 8;
			result |= srcData[index + 1];
			result <<= 8;
			result |= srcData[index];
			return result;
		}


		void PushDWUint8(std::vector<uint8_t>& data, uint32_t number) {
			for (uint32_t i = 0; i < 4; i++) {
				uint8_t bitmask = 0xFF;
				bitmask &= number;
				number >>= 8;
				data.push_back(bitmask);
			}
		}

		void PushQWUint8(std::vector<uint8_t>& data, uint64_t number) {
			for (uint32_t i = 0; i < 8; i++) {
				uint8_t bitmask = 0xFF;
				bitmask &= number;
				number >>= 8;
				data.push_back(bitmask);
			}
		}

		void RWVecDWUint8(std::vector<uint8_t>& data, uint32_t number, size_t pos) {
			for (uint32_t i = 0; i < 4; i++) {
				uint8_t bitmask = 0xFF;
				bitmask &= number >> i * 8;
				data[pos] = bitmask;
				pos++;
			}
		}

		void RWVecQWUint8(std::vector<uint8_t>& data, uint64_t number, size_t pos) {
			for (uint32_t i = 0; i < 8; i++) {
				uint8_t bitmask = 0xFF;
				bitmask &= number >> i * 8;
				data[pos] = bitmask;
				pos++;
			}
		}

		std::vector<uint8_t> DeepCopyU8(const std::vector<uint8_t>& srcData, size_t index, size_t size) {
			return std::vector<uint8_t>(srcData.begin() + index, srcData.begin() + index + size);
		}

		std::string GenerateVecU8ToCPPcode(const std::vector<uint8_t>& srcData) {
			std::stringstream buf;
			buf << std::hex;

			buf << '{';

			for (const uint16_t v : srcData) {//向上转型使得其内部可以正确的处理为16进制的字符串
				buf << "0x";
				buf << v;
				buf << ',';
			}

			std::string result = buf.str();
			result.pop_back();
			result += "}";

			return result;
		}

		bool WriteBinFile(const std::string& path, const std::vector<uint8_t>& BinData) {
			std::ofstream outputFile(path, std::ios::binary | std::ios::trunc);
			bool result = outputFile.is_open();
			if (!result) {
				return result;
			}
			outputFile.write((const char*)BinData.data(), BinData.size());
			outputFile.close();
			return result;
		}

		std::optional<std::vector<uint8_t>> ReadBinFile(const std::string& path) {
			std::ifstream inputFile(path, std::ios::binary | std::ios::out);
			if (!inputFile.is_open()) {//未成功打开 
				return std::nullopt;
			}
			inputFile.seekg(0, std::ios::end);//移动文件指针以获得文件大小
			size_t fileSize = static_cast<size_t>(inputFile.tellg());
			inputFile.seekg(0, std::ios::beg);

			std::vector<uint8_t> result(fileSize);
			inputFile.read((char*)result.data(), fileSize);
			return result;
		}

		uint32_t VecU8Reader::GetDoubleWord() {
			if (index + 3 >= data.size()) {
				throw std::out_of_range("VecU8Reader: invalid vector read");
			}
			uint32_t result = GetU8VecDW(data, index);
			index += 4;
			return result;
		}

		uint64_t VecU8Reader::GetQuadWord() {
			if (index + 7 >= data.size()) {
				throw std::out_of_range("VecU8Reader: invalid vector read");
			}
			uint64_t result = GetU8VecQW(data, index);
			index += 8;
			return result;
		}

		uint8_t VecU8Reader::GetByte() {
			if (index >= data.size()) {
				throw std::out_of_range("VecU8Reader: invalid vector read");
			}
			uint8_t result = data[index];
			index++;
			return result;
		}
	}
}
