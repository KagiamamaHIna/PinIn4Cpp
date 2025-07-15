/*
	本文件只是简单的拼音测试用例，不是核心代码库之一
	你如果想要使用搜索功能，你应该用TreeSearcher.h获取拼音匹配支持
	如果你只需要拼音获取，那么本项目的PinIn也是非常合适的

	项目统一用标准的std::string和std::string_view处理字符串，包括utf8字符串，所以你要保证你输入的应该是utf8字符串
	所以这是一个天然支持utf8且只支持utf8的库，原始的Java PinIn库有个不可忽视的缺陷，就是不支持utf16不可表达的字
*/

#include <iostream>
#include <fstream>
#include <chrono>

#include "TreeSearcher.h"

static long long GetTimestampMS() {//获取当前毫秒的时间戳
	auto now = std::chrono::steady_clock::now();
	//将时间点转换为时间戳
	return std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
}

int main() {
	system("chcp 65001");//编码切换，windows平台的cmd命令
	system("pause");
	PinInCpp::TreeSearcher tree(PinInCpp::Logic::CONTAIN, "pinyin.txt");
	//第二个参数为拼音数据的文件路径，请使用https://github.com/mozillazg/pinyin-data中提供的，当然本项目也放有pinyin.txt，你可以直接使用
	//说起来这个数据源是原本的约三倍大小（
	//不过格式方面不一样，所以不方便比较

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

	//tree.GetPinIn().SetCharCache(true); //默认开启
	std::fstream file("small.txt");
	std::string line;
	long long now = GetTimestampMS();
	while (std::getline(file, line)) {
		tree.put(line);
	}
	tree.GetPinIn().SetCharCache(true);//手动关闭可以释放缓存，不过搜索时也可能会利用缓存，虽然性能下降并不明显
	long long end = GetTimestampMS();
	std::cout << end - now << '\n';
	//插入耗时，比Java的快了，目前提供的缓存支持，主要原因还是在utf8字符串处理之类的问题上，当然内存占用也是如此(更大)，毕竟utf8比utf16浪费内存，而且有std::string作为key的开销
	//或许可以考虑用char32_t存单字符，不过这改动可就大了
	//而且为了保证内存池的utf8字符串O1的随机访问，我实现了一个UTF8StringPool::chars_offset成员，必不可少但是也耗费更多内存了
	//内存占用的问题部分还来源size_t类型，因为64位下是八字节大的，基本上是翻倍了

	while (true) {//死循环，你可以随便搜索测试集的内容用于测试
		std::cout << "input:";
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
