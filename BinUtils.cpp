#include "BinUtils.h"

namespace PinInCpp {
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
}
