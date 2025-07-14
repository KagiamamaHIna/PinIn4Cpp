#include <iostream>
#include "PinIn.h"
#include <fstream>
#include "TreeSearcher.h"
#include <chrono>

static long long GetTimestampMS() {//获取当前毫秒的时间戳
	auto now = std::chrono::steady_clock::now();
	//将时间点转换为时间戳
	return std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
}
#include "Keyboard.h"
/*
TODO:
拼音格式化类应该用一个函数实现，其重量级的实现放在cpp文件中，用枚举类确定其行为
*/
int main() {
	system("chcp 65001");

	/*size_t id1 = test.put("你好！");
	size_t id2 = test.put("草uckyou!");
	size_t id3 = test.put("逆天");

	std::cout << id1 << '\n';
	std::cout << id2 << '\n';
	std::cout << id3 << '\n';

	std::cout << test.getstr_view(id2);

	PinInCpp::PinIn testPin("D:/repos/PinyinTest/pinyin.txt");*/
	system("pause");
	PinInCpp::TreeSearcher tree(PinInCpp::Logic::CONTAIN, "D:/repos/PinyinTest/pinyin.txt");
	PinInCpp::PinIn::Config cfg = tree.GetPinIn().config();
	cfg.fZh2Z = true;
	cfg.fSh2S = true;
	cfg.fCh2C = true;
	cfg.fAng2An = true;
	cfg.fIng2In = true;
	cfg.fEng2En = true;
	cfg.fU2V = true;
	cfg.commit();

	std::fstream file("D:/repos/PinyinTest/small.txt");
	std::string line;
	long long now = GetTimestampMS();
	while (std::getline(file, line)) {
		tree.put(line);
	}
	long long end = GetTimestampMS();
	//std::cout << end - now << '\n';

	now = GetTimestampMS();
	for (const auto& v : tree.ExecuteSearchView("lizihf")) {
		std::cout << v << '\n';
	}
	end = GetTimestampMS();

	//std::cout << end - now << '\n';

	/*std::cout << (b[0] == '\0') << '\n';*/

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


	system("pause");
}
