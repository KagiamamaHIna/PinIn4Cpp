# PinIn for C++
一个用于解决各类汉语拼音匹配问题的 C++ 库，本质上是Java [PinIn](https://github.com/Towdium/PinIn) 项目的C++移植和改造，使用标准C++编写，无第三方依赖，需配置项目为C++20编译，支持CMake

搜索实现方面，目前只移植了TreeSearcher，其他的没计划也不会移植

除此之外，它也和原版一样可以将汉字转换为拼音字符串，包括 ASCII，Unicode 和注音符号

> 双拼输入同样尚在测试阶段，并且不（也不会）支持字形码。重码过多时，可以使用声调作为辅助码。

## 特性
基本上保留了大部分原有的特性，不过要注意一些区别
- 支持UTF8字符串处理而不是UTF16 (也只支持UTF8)
- - [相当于解决了PinIn这个issue3的问题，因为原始设计只能使用utf16](https://github.com/Towdium/PinIn/issues/3)
- 多了首字母模糊音匹配功能
- - [相当于PinIn这个issue1的解决方案](https://github.com/Towdium/PinIn/issues/1)
- 只实现了TreeSearcher
- 提供了新的ParallelSearch类，内置线程池机制的并行化树搜索，在数据量很大时可以提供更好的即时搜索性能
- 提供了PinIn类、TreeSearcher类、ParallelSearch类的序列化和反序列化接口，以提供快速加载已有结构的功能
- 提供了[C API](PinInCAPI.h)，通过C API可以轻松的被支持FFI的语言调用，或者是用C语言使用这个库
- - C API的使用有一个LuaJIT FFI的[示例](FFIExample/PinInLua.lua)，这个示例是一个完整的Lua封装并且带有EmmyLua风格的类型注解

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
| 64位 | 63.7471ms | 15.69MB |
| 32位 | 61.6172ms | 8.79MB |

__前缀匹配__
| 环境 | 构建耗时(100次构建树平均耗时) | 内存使用 |
|:------:|:------:|:------|
| 64位 | 8.33242ms | 3.40MB |
| 32位 | 8.62215ms | 2.61MB |

内存测试使用Visual Studio 2022的堆分析得出的

## 示例
你可以轻松使用CMake将本项目导入到你的CMake项目中。
### 方案1：
```cmake
if(MSVC) # MSVC一定要设置为utf-8编译，因为原始代码有utf8字符，而MSVC默认是随系统的会导致问题
    add_compile_options(/utf-8)
endif()

add_subdirectory(PinIn4Cpp) # 应为本项目的CMakeLists.txt所在的文件夹路径（就是源代码）
target_link_libraries(YourProject PRIVATE PinIn4Cpp::PinIn4Cpp) # YourProject是你的项目，请自行更改
```
### 方案2
```cmake
if(MSVC) # MSVC一定要设置为utf-8编译，因为原始代码有utf8字符，而MSVC默认是随系统的会导致问题
    add_compile_options(/utf-8)
endif()

# 对本项目使用cmake CMakeLists.txt
# 然后用："cmake --build . --config Release" 之类的指令build
# 然后用这样的语句install："cmake --install . --config Release"
# 可以手动通过cmake . -DBUILD_SHARED_LIBS=ON/OFF 来开关是否编译动态库
# 就可以像这样子去使用了
find_package(PinIn4Cpp)
target_link_libraries(YourProject PRIVATE PinIn4Cpp::PinIn4Cpp)
```
本项目使用标准C++编写且无依赖，即使不想依赖CMake也可以自行选择其他方式编译，但是需要处理PinInCAPI.h的导出宏。
### 使用展示
下面的代码简单的展示了本项目的基础使用方式:
```cpp
#include <iostream>
#include "PinIn4Cpp/TreeSearcher.h"

int main() {
#ifdef _WIN32
	system("chcp 65001");//这个用于切换cmd的代码页，让windows环境下可以显示utf8字符串
#endif

	PinInCpp::TreeSearcher TreeA(PinInCpp::Logic::CONTAIN, "pinyin.txt");//路径是拼音字典的路径
	std::shared_ptr<PinInCpp::PinIn> pinin = TreeA.GetPinInShared();//返回其共享所有权的智能指针

	PinInCpp::TreeSearcher TreeB(PinInCpp::Logic::BEGIN, pinin);//通过传递智能指针，现在A和B树的拼音上下文是共享的

	TreeA.put("测试文本");
	for (const auto& v : TreeA.ExecuteSearch("wenben")) {
		std::cout << v << std::endl;
	}
	PinInCpp::PinIn::Config cfg = pinin->config();//更改PinIn类的配置
	cfg.fFirstChar = true;//新增的首字母模糊
	cfg.commit();

	for (const auto& v : TreeA.ExecuteSearch("wb")) {
		std::cout << v << std::endl;
	}
}
```
更多细节请查看[PinyinTest.cpp](example/PinyinTest.cpp)。

## 致谢
[PinIn](https://github.com/Towdium/PinIn)库的开发者，有你们的代码才有的这个项目！

拼音数据来源: [pinyin-data](https://github.com/mozillazg/pinyin-data) 无改造，按原样提供
