#pragma once
#include <vector>

namespace PinInCpp {
	void PushDWUint8(std::vector<uint8_t>& data, uint32_t number);
	void PushQWUint8(std::vector<uint8_t>& data, uint64_t number);
	void RWVecDWUint8(std::vector<uint8_t>& data, uint32_t number, size_t pos);
	void RWVecQWUint8(std::vector<uint8_t>& data, uint64_t number, size_t pos);
	uint32_t GetU8VecDW(const std::vector<uint8_t>& srcData, size_t index);
	uint64_t GetU8VecQW(const std::vector<uint8_t>& srcData, size_t index);
	std::vector<uint8_t> DeepCopyU8(const std::vector<uint8_t>& srcData, size_t index, size_t size);
}
