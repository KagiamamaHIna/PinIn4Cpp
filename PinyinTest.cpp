#include <iostream>
#include "PinIn.h"
#include <fstream>
#include "TreeSearcher.h"
#include <chrono>


#include "Keyboard.h"

/*
TODO:
拼音格式化类应该用一个函数实现，其重量级的实现放在cpp文件中，用枚举类确定其行为
*/

int main() {
	system("chcp 65001");
	/*PinInCpp::StringPool test;

	size_t id1 = test.put("你好！");
	std::cout << test.getLastStrUTF8Size() << '\n';
	size_t id2 = test.put("草uckyou!");
	std::cout << test.getLastStrUTF8Size() << '\n';
	size_t id3 = test.put("逆天");
	for (const auto& v : test.offsets()) {
		std::cout << v << '\n';
	}
	std::cout << id1 << '\n';
	std::cout << id2 << '\n';
	std::cout << id3 << '\n';

	std::cout << test.getchar(id2);*/
	PinInCpp::PinIn testPin("D:/repos/PinyinTest/pinyin.txt");
	size_t id = testPin.GetPinyinId("调");
	std::cout << testPin.Test(id + 6) << '\n';
	for (const auto& str : testPin.GetPinyinById(id, true)) {
		std::cout << str << '\n';
	}

	system("pause");

	/*auto cfg = testPin.config();
	cfg.keyboard = PinInCpp::Keyboard::DAQIAN;
	cfg.commit();

	auto vec = testPin.GetPinyinView("圈", true);

	for (const auto& str : vec) {
		std::cout << str << ':';
		for (const auto& s : testPin.getkeyboard().split(str)) {
			std::cout << s << ' ';
		}
		std::cout << std::endl;
	}*/

	//PinInCpp::PinIn testPin("D:/repos/PinyinTest/pinyin.txt");
	//for (const auto& str : testPin.GetPinyinView("佻", true)) {
	//	std::cout << str << '\n';
	//}
	/*std::fstream file("D:/repos/PinyinTest/small.txt");
	std::string line;
	while (std::getline(file, line)) {
		test.put(line);
	}*/

	system("pause");
}
