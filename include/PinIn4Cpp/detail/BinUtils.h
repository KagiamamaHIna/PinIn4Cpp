#pragma once
#include <vector>
#include <sstream>
#include <fstream>
#include <optional>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>

namespace PinInCpp {
	//把std::vector<uint8_t>生成为一份C++的代码，内容就是关于数据初始化的结构
	//大概是这样的: {0xAA,0xBB,...,0xFF} 只生成声明
	//可以方便开发者内嵌数据
	//警告：太大的文件会严重拖累编译速度，甚至让你的代码分析器死机，数据量过大请谨慎选择此方案
	//从我作者本人的视角来看，我非常不推荐使用这个，除非你的字典真的很小
	std::string GenerateVecU8ToCPPcode(const std::vector<uint8_t>& srcData);

	namespace detail {
		void PushDWUint8(std::vector<uint8_t>& data, uint32_t number);
		void PushQWUint8(std::vector<uint8_t>& data, uint64_t number);
		void RWVecDWUint8(std::vector<uint8_t>& data, uint32_t number, size_t pos);
		void RWVecQWUint8(std::vector<uint8_t>& data, uint64_t number, size_t pos);
		uint32_t GetU8VecDW(const std::vector<uint8_t>& srcData, size_t index);
		uint64_t GetU8VecQW(const std::vector<uint8_t>& srcData, size_t index);
		std::vector<uint8_t> DeepCopyU8(const std::vector<uint8_t>& srcData, size_t index, size_t size);

		bool WriteBinFile(const std::string& path, const std::vector<uint8_t>& BinData);//写入二进制文件，返回的是 是否写入成功
		std::optional<std::vector<uint8_t>> ReadBinFile(const std::string& path);//读取二进制文件

		//负责观察一个u8向量的数据并读取
		class VecU8Reader {
		public:
			VecU8Reader(const std::vector<uint8_t>& data, size_t index = 0) :data{ data }, index{ index } {}
			uint32_t GetDoubleWord();
			uint64_t GetQuadWord();
			uint8_t GetByte();
			size_t GetSizeTFromQW() {
				return static_cast<size_t>(GetQuadWord());
			}

			const std::vector<uint8_t>& GetData()const noexcept {
				return data;
			}
			size_t GetIndex()const noexcept {
				return index;
			}
			void AddIndex(size_t num) {
				index += num;
			}
		private:
			const std::vector<uint8_t>& data;
			size_t index;
		};
	}
}
