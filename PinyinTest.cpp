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
#include <array>

#include "TreeSearcher.h"

using high_time_point = std::chrono::high_resolution_clock::time_point;

static high_time_point GetTimePoint() {
	return std::chrono::high_resolution_clock::now();
}

static double GetTimeMS(high_time_point start, high_time_point end) {//微秒
	std::chrono::duration<double> result = end - start;
	return result.count() * 1000;
}

constexpr int TreeLoopInsertCount = 1;
constexpr int SearcherLoopCount = 1;

int main() {
	system("chcp 65001");//编码切换，windows平台的cmd命令

	std::fstream file("small.txt");//数据读取
	std::string line;
	std::vector<std::string> FileCache;
	while (std::getline(file, line)) {
		FileCache.push_back(line);
	}
	file.close();
	//PinIn的构造函数参数为拼音数据的文件路径，请使用https://github.com/mozillazg/pinyin-data中提供的，当然本项目也放有pinyin.txt，你可以直接使用
	//说起来这个数据源是原本的约三倍大小（
	//不过格式方面不一样，所以不方便比较
	//TreeSearcher的第二个参数除了智能指针，其实都是PinIn类的构造参数

	system("pause");
	high_time_point now = GetTimePoint();
	std::shared_ptr<PinInCpp::PinIn> pininptr = std::make_shared<PinInCpp::PinIn>("pinyin.txt");
	high_time_point end = GetTimePoint();
	std::cout << GetTimeMS(now, end) << "ms\n";//计算获取耗时并打印，单位毫秒

	system("pause");
	//pininptr->SetCharCache(true); //默认开启
	PinInCpp::PinIn::Config cfg = pininptr->config();
	cfg.keyboard = PinInCpp::Keyboard::DAQIAN;
	cfg.fZh2Z = true;
	cfg.fSh2S = true;
	cfg.fCh2C = true;
	cfg.fAng2An = true;
	cfg.fIng2In = true;
	cfg.fEng2En = true;
	cfg.fU2V = true;
	cfg.fFirstChar = true;//新增的首字母模糊
	cfg.commit();

	double timeCount = 0;
	std::unique_ptr<PinInCpp::TreeSearcher> tree;

	for (int i = 0; i < TreeLoopInsertCount; i++) {
		tree = std::make_unique<PinInCpp::TreeSearcher>(PinInCpp::Logic::CONTAIN, pininptr);
		for (const auto& v : FileCache) {
			high_time_point now = GetTimePoint();
			tree->put(v);
			high_time_point end = GetTimePoint();
			timeCount += GetTimeMS(now, end);
		}
	}

	std::cout << timeCount / (double)TreeLoopInsertCount << "ms\n";

	//pininptr->SetCharCache(false);//手动关闭可以释放缓存，不过搜索时也可能会利用缓存，会导致一定程度的性能下降

	//插入耗时，比Java的快了，目前提供了缓存支持，主要原因还是在utf8字符串处理之类的问题上，当然内存占用也是如此(更大)，毕竟utf8比utf16浪费内存，而且有std::string作为key的开销
	//目前已利用FourCC技术，将单UTF8字符高效的打包成uint32_t，利用缓冲区技术快速解包回去，实现单字符key的高效存储，避免字符串哈希
	//而且为了保证内存池的utf8字符串O1的随机访问，我实现了一个UTF8StringPool::chars_offset成员，必不可少但是也耗费更多内存了
	//内存占用的问题部分还来源size_t类型，因为64位下是八字节大的，基本上是翻倍了

	while (true) {//死循环，你可以随便搜索测试集的内容用于测试
		std::cout << "input:";
		std::getline(std::cin, line);

		double timeCount = 0;
		std::vector<std::string_view> vec;
		for (int i = 0; i < SearcherLoopCount; i++) {
			high_time_point now = GetTimePoint();
			vec = tree->ExecuteSearchView(line);
			high_time_point end = GetTimePoint();
			timeCount += GetTimeMS(now, end);
		}

		for (const auto& v : vec) {
			std::cout << v << '\n';
		}
		std::cout << timeCount / (double)SearcherLoopCount << "ms\n";//计算获取耗时并打印，单位毫秒
	}
}
