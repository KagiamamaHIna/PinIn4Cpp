#pragma once
#include <string>
#include <vector>

#include "PinIn.h"


namespace PinInCpp {
	/*
	Compressor

	目前设计支持只支持UTF8的可变长编码，这是一个特化的，不可编辑的字符串池
	*/
	class UTF8StringPool {
	public:
		UTF8StringPool();

		/*virtual const std::vector<size_t>& offsets()const {
			return strs_offset;
		}*/
		bool end(size_t i)const {
			return strs[chars_offset[i]] == '\0';
		}
		size_t put(const std::string_view& s);//返回的是其插入完成后字符串首端索引
		std::string getchar(size_t i)const;//获取指定字符
		std::string getstr(size_t strStart)const;//输入首端索引构造完整字符串
		std::string_view getchar_view(size_t i)const;//获取指定字符的只读视图 持有时不要变动字符串池！
		std::string_view getstr_view(size_t strStart)const;//输入首端索引构造完整字符串的只读视图 持有时不要变动字符串池！
		uint32_t getcharFourCC(size_t i)const;//针对单字符的FourCC打包编码的实现
		size_t getLastStrSize()const {//获取上一个插入的UTF8字符串的长度
			return last_size;
		}
		//单位是字节
		void reserve(size_t _Newcapacity) {
			strs.reserve(_Newcapacity);
		}
		bool EqualChar(size_t indexA, size_t indexB)const;
		void ShrinkToFit() {
			strs.shrink_to_fit();
		}
	private:
		std::vector<char> strs;//字节数组，用于将多个字符串(字节流)放入容器中，避免内存碎片
		//std::vector<size_t> strs_offset;//表示每组字符串的宽度偏移量
		size_t last_offset = 0;//替代设计
		size_t last_size = 0;
		std::vector<size_t> chars_offset;//索引表示的为字符的位置，值表示的是字符的末尾，用上一个值代表字符的开始
	};
}
