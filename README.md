# PinIn for C++
一个用于解决各类汉语拼音匹配问题的 C++ 库，本质上是Java [PinIn](https://github.com/Towdium/PinIn) 项目的C++移植和改造，使用标准C++编写，无第三方依赖，需配置项目为C++20编译

搜索实现方面，目前只移植了TreeSearcher，其他的没计划也不会移植

除此之外，它也和原版一样可以将汉字转换为拼音字符串，包括 ASCII，Unicode 和注音符号

> 双拼支持不佳且有bug，目前不推荐使用，全拼实际上也不怎么影响嘛反正。

## 特性
基本上保留了大部分原有的特性，不过要注意一些区别
- 支持UTF8字符串处理而不是UTF16 (也只支持UTF8)
- - [相当于解决了PinIn这个issue3的问题，因为原始设计只能使用utf16](https://github.com/Towdium/PinIn/issues/3)
- 多了首字母模糊音匹配功能
- - [相当于PinIn这个issue1的解决方案](https://github.com/Towdium/PinIn/issues/1)
- 只实现了TreeSearcher
- 提供了新的ParallelSearch类，内置线程池机制的并行化树搜索，在数据量很大时可以提供更好的即时搜索性能

搜索方面应该和原版无异

## 性能
性能方面因为支持UTF8字符串处理，导致理论性能有所下降，同时因为使用size_t这样的跟随环境大小的类型，在64位环境下内存占用会更多

而且拼音数据也比Java版的更加全面，这个也会使用更多内存

因此本库的性能不能和Java版的比较，他们本质上处理的数据发生了变化，只是搜索算法是一致的，而且堆内存分配真的很慢（

本库的PinyinTest.cpp是一个非常简单的测试样例和使用案例，数据一样是Enigmatica导出的[样本](small.txt)

CPU为i9-14900HX 简单点来说性能大概如下，搜索耗时和输入字符串存在很大关系，不列举：

__部分匹配__
| 环境 | 构建耗时(100次构建树平均耗时) | 内存使用 |
|:------:|:------:|:------|
| 64位 | 81.7664ms | 27.69MB |
| 32位 | 83.775ms | 15.46MB |

__前缀匹配__
| 环境 | 构建耗时(100次构建树平均耗时) | 内存使用 |
|:------:|:------:|:------|
| 64位 | 19.8722ms | 8.12MB |
| 32位 | 20.7908ms | 4.64MB |

其实内存使用也测的非常不严谨，我拿任务管理器测的，没计算PinIn的内存开销（

## 示例
下面的代码简单的展示了本项目的基础使用方式:
```cpp
#include <iostream>
#include "TreeSearcher.h"

int main() {
	system("chcp 65001");//这是仅windows平台的测试，这个用于切换cmd的代码页，让其可以显示utf8字符串

	PinInCpp::TreeSearcher TreeA(PinInCpp::Logic::CONTAIN, "pinyin.txt");//路径是拼音字典的路径
	std::shared_ptr<PinInCpp::PinIn> pinin = TreeA.GetPinInShared();//返回其共享所有权的智能指针

	PinInCpp::TreeSearcher TreeB(PinInCpp::Logic::BEGIN, pinin);//通过传递智能指针，现在A和B树的拼音上下文是共享的

	TreeA.put("测试文本");
	for (const auto& v : TreeA.ExecuteSearchView("wenben")) {
		//View后缀为只读视图版本，性能更高，但是使用时不要改变树的状态，如ExecuteSearchView和put都有可能改变，这时候视图有失效风险
		std::cout << v << std::endl;
	}
	PinInCpp::PinIn::Config cfg = pinin->config();//更改PinIn类的配置
	cfg.fFirstChar = true;//新增的首字母模糊
	cfg.commit();

	for (const auto& v : TreeA.ExecuteSearchView("wb")) {
		std::cout << v << std::endl;
	}
}
```
更多细节请查看[PinyinTest.cpp](PinyinTest.cpp)。

你可以很简单的包括到你的C++项目中，因为他使用标准C++编写且无依赖，只需要注意拿C++20编译即可。

## 致谢
[PinIn](https://github.com/Towdium/PinIn)库的开发者，有你们的代码才有的这个项目！

拼音数据来源: [pinyin-data](https://github.com/mozillazg/pinyin-data) 无改造，按原样提供
