/*
	本文件只是简单的拼音测试用例，不是核心代码库之一
	你如果想要使用搜索功能，你应该用TreeSearcher.h获取拼音匹配支持
	如果你只需要拼音获取，那么本项目的PinIn也是非常合适的
*/

#include <iostream>
#include "PinIn.h"
#include <fstream>
#include <chrono>

#include "TreeSearcher.h"

static long long GetTimestampMS() {//获取当前毫秒的时间戳
	auto now = std::chrono::steady_clock::now();
	//将时间点转换为时间戳
	return std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
}

int main() {
	system("chcp 65001");//编码切换

	PinInCpp::TreeSearcher tree(PinInCpp::Logic::CONTAIN, "pinyin.txt");
	PinInCpp::PinIn::Config cfg = tree.GetPinIn().config();
	cfg.fZh2Z = true;
	cfg.fSh2S = true;
	cfg.fCh2C = true;
	cfg.fAng2An = true;
	cfg.fIng2In = true;
	cfg.fEng2En = true;
	cfg.fU2V = true;
	cfg.fFirstChar = true;//新增的首字母模糊
	cfg.commit();

	std::fstream file("small.txt");
	std::string line;
	long long now = GetTimestampMS();
	while (std::getline(file, line)) {
		tree.put(line);
	}
	long long end = GetTimestampMS();
	std::cout << end - now << '\n';
	//插入耗时，比Java的慢，主要原因还是在utf8字符串处理之类的问题上，当然内存占用也是如此(更大)，utf8比utf16浪费内存
	//项目统一用标准的std::string和std::string_view处理字符串，包括utf8字符串
	//所以这是一个天然支持utf8且只支持utf8的库，原始的Java PinIn库有个不可忽视的缺陷，就是不支持utf16不可表达的字

	while (true) {//死循环，你可以随便搜索测试集的内容用于测试
		std::getline(std::cin, line);
		now = GetTimestampMS();
		auto vec = tree.ExecuteSearchView(line);
		end = GetTimestampMS();
		for (const auto& v : vec) {
			std::cout << v << '\n';
		}
		std::cout << end - now << '\n';//计算获取耗时并打印，单位毫秒
	}
}
