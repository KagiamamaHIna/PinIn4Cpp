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
	PinInCpp::UTF8StringPool test;

	/*size_t id1 = test.put("你好！");
	size_t id2 = test.put("草uckyou!");
	size_t id3 = test.put("逆天");

	std::cout << id1 << '\n';
	std::cout << id2 << '\n';
	std::cout << id3 << '\n';

	std::cout << test.getstr_view(id2);

	PinInCpp::PinIn testPin("D:/repos/PinyinTest/pinyin.txt");*/

	std::string a("\0\0\0\0\0");

	std::string_view b(a.data(), a.size());
	/*std::cout << (b[0] == '\0') << '\n';*/

	system("pause");
	/*PinInCpp::PinIn::Character Char = testPin.GetChar("栓");
	for (const auto& py : Char.GetPinyins()) {
		std::cout << py.ToString() << '\n';
	}*/
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
