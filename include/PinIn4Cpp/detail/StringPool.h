#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstdint>

namespace PinInCpp {
	namespace detail {
		/*
		Compressor

		目前设计支持只支持UTF8的可变长编码，这是一个特化的，不可编辑的字符串池
		*/
		class UTF8StringPool {
		public:
			bool end(size_t i)const noexcept {
				return strs[i] == '\0';
			}
			size_t put(std::string_view s);//返回的是其插入完成后字符串首端索引
			std::string getchar(size_t i)const;//获取指定字符
			std::string getstr(size_t strStart)const;//输入首端索引构造完整字符串
			size_t getStrSize(size_t strStart)const;
			int PutToCharBuf(size_t strStart, char* buf, size_t bufSize);
			//std::string_view getchar_view(size_t i)const noexcept;//获取指定字符的只读视图 持有时不要变动字符串池！
			//std::string_view getstr_view(size_t strStart)const noexcept;//输入首端索引构造完整字符串的只读视图 持有时不要变动字符串池！
			uint32_t getcharFourCC(size_t i)const noexcept {
				return strs[i];
			}
			size_t getLastOffset()const noexcept {//获取上次插入完成后的偏移值
				return strs.size();
			}
			//单位是四字节
			void reserve(size_t _Newcapacity) {
				strs.reserve(_Newcapacity);
			}
			bool EqualChar(size_t indexA, size_t indexB)const noexcept {//索引相等那必然相等，那么如果索引相等了，那么就不需要去访问内存了，反之需要。
				return indexA == indexB || strs[indexA] == strs[indexB];
			}
			void ShrinkToFit() {
				strs.shrink_to_fit();
			}
			//32位环境和64位环境生成的文件格式应该是通用的，但是如果64位下数据量过大，32位环境下加载有潜在的溢出导致逻辑错误的风险
			//你应该只传入对应类的Serialization方法生成的数据，传入一个不正确格式的很有可能会造成未定义行为
			//不过有可能抛出std::out_of_range，抛出这个代表读取时越界了，通常意味着结构错误
			//抛出BinaryVersionInvalidException代表版本号错误，不可使用
			static UTF8StringPool Deserialize(const std::vector<uint8_t>& data, size_t index = 0);
			static std::optional<UTF8StringPool> DeserializeFromFile(std::string_view path, size_t index = 0);
			std::vector<uint8_t> Serialize()const;
			//返回真代表写入成功
			bool SerializeToFile(std::string_view path)const;
		private:
			//看上去很蠢，把可变长编码的字符串当作一个固定的uint32_t存储了，实际上这样子比老办法的索引要更加的优秀
			//因为TreeSearcher依赖O1的字符串随机访问，老办法的索引需要先获取字符长度，才能去按字节读取数据
			//索引本身也是一次占用开销，64位下是8字节大的，这远比直接存uint32_t浪费
			//就算是32位下，还要存储原始字符的字节，还是有浪费
			//最极致的办法就是这个看似蠢的办法上，把可变长编码的字符串当成固定的uint32_t来存储
			//而且少了一次间接寻址，之前的索引方法本质是间接寻址
			//内部的依赖fourCC编码的处理也变得更快了
			//唯一缺点是不能提供视图接口了
			std::vector<uint32_t> strs;
		};
	}
}
