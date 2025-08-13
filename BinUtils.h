#pragma once
#include <vector>
#include <sstream>

namespace PinInCpp {
	void PushDWUint8(std::vector<uint8_t>& data, uint32_t number);
	void PushQWUint8(std::vector<uint8_t>& data, uint64_t number);
	void RWVecDWUint8(std::vector<uint8_t>& data, uint32_t number, size_t pos);
	void RWVecQWUint8(std::vector<uint8_t>& data, uint64_t number, size_t pos);
	uint32_t GetU8VecDW(const std::vector<uint8_t>& srcData, size_t index);
	uint64_t GetU8VecQW(const std::vector<uint8_t>& srcData, size_t index);
	std::vector<uint8_t> DeepCopyU8(const std::vector<uint8_t>& srcData, size_t index, size_t size);

	//把std::vector<uint8_t>生成为一份C++的代码，内容就是关于数据初始化的结构
	//大概是这样的: {0xAA,0xBB,...,0xFF} 只生成声明
	//可以方便开发者内嵌数据
	//警告：太大的文件会严重拖累编译速度，甚至让你的代码分析器死机，数据量过大请谨慎选择此方案
	//从我作者本人的视角来看，我非常不推荐使用这个，除非你的字典真的很小
	std::string GenerateVecU8ToCPPcode(const std::vector<uint8_t>& srcData);
}
