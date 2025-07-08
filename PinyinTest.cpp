#include <iostream>
#include "PinIn.h"
#include <fstream>
#include "TreeSearcher.h"

#include "Keyboard.h"

struct ToneData {
	char c;
	uint8_t tone;
};

int main() {
	system("chcp 65001");

	//for (const auto& str : testPin.GetPinyin("佻", false)) {
	//	std::cout << str << '\n';
	//}

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
	system("pause");
	PinInCpp::PinIn testPin("D:/repos/PinyinTest/pinyin.txt");

	/*std::fstream file("D:/repos/PinyinTest/small.txt");
	std::string line;
	while (std::getline(file, line)) {
		test.put(line);
	}*/

	system("pause");
}
