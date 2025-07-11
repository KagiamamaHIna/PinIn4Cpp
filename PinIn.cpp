#include "PinIn.h"

const inline static std::unordered_map<std::string_view, std::vector<std::string_view>> FUZZY_PAIRS = {
	{"zh", {"z"}}, {"z", {"zh"}},
	{"ch", {"c"}}, {"c", {"ch"}},
	{"sh", {"s"}}, {"s", {"sh"}},
	{"ang", {"an"}}, {"an", {"ang"}},
	{"eng", {"en"}}, {"en", {"eng"}},
	{"ing", {"in"}}, {"in", {"ing"}},
	{"v", {"u"}}, {"ve", {"ue"}}, {"van", {"uan"}}, {"vn", {"un"}}
};

namespace PinInCpp {
	//函数定义
	//Unicode码转utf8字节流
	std::string UnicodeToUtf8(char32_t unicodeChar) {
		std::string utf8;
		if (unicodeChar <= 0x7F) {
			//1字节数据
			utf8 += static_cast<char>(unicodeChar);
		}
		else if (unicodeChar <= 0x7FF) {
			//2字节数据
			utf8 += static_cast<char>(0xC0 | ((unicodeChar >> 6) & 0x1F));
			utf8 += static_cast<char>(0x80 | (unicodeChar & 0x3F));
		}
		else if (unicodeChar <= 0xFFFF) {
			//3字节数据
			utf8 += static_cast<char>(0xE0 | ((unicodeChar >> 12) & 0x0F));
			utf8 += static_cast<char>(0x80 | ((unicodeChar >> 6) & 0x3F));
			utf8 += static_cast<char>(0x80 | (unicodeChar & 0x3F));
		}
		else if (unicodeChar <= 0x10FFFF) {
			//4字节数据
			utf8 += static_cast<char>(0xF0 | ((unicodeChar >> 18) & 0x07));
			utf8 += static_cast<char>(0x80 | ((unicodeChar >> 12) & 0x3F));
			utf8 += static_cast<char>(0x80 | ((unicodeChar >> 6) & 0x3F));
			utf8 += static_cast<char>(0x80 | (unicodeChar & 0x3F));
		}
		return utf8;
	}

	int HexStrToInt(const std::string& str) {
		return std::stoi(str, nullptr, 16);
	}

	std::vector<std::string> split(const std::string& str, char delimiter) {
		std::vector<std::string> result;
		size_t start = 0;
		size_t end = str.find(delimiter);

		while (end != std::string::npos) {
			result.push_back(str.substr(start, end - start));
			start = end + 1;
			end = str.find(delimiter, start);
		}

		result.push_back(str.substr(start));
		return result;
	}

	size_t PinIn::CharPool::put(const std::string_view& s) {
		size_t result = strs.size();
		strs.insert(strs.end(), s.begin(), s.end());//插入字符串
		return result;
	}

	size_t PinIn::CharPool::putChar(const char s) {
		size_t result = strs.size();
		strs.push_back(s);
		return result;
	}

	void PinIn::CharPool::putEnd() {
		strs.push_back('\0');
	}

	std::vector<std::string> PinIn::CharPool::getPinyinVec(size_t i)const {//根据理论上的正确格式来讲，应当是用','字符分隔拼音，然后用'\0'作为拼音数据末尾
		//编辑i当作索引id即可
		size_t cursor = 0;//直接在result上构造字符串，用这个代表当前访问的字符串
		std::vector<std::string> result(1);//提前构造一个字符串

		char tempChar = strs[i];//局部拷贝，避免多次访问
		while (tempChar) {//结尾符就退出
			if (tempChar == ',') {//不保存这个，压入下一个空字符串，移动cursor
				result.push_back("");
				cursor++;
			}
			else {
				result[cursor].push_back(tempChar);
			}
			i++;
			tempChar = strs[i];//自增完成后再取下一个字符
		}
		return result;
	}

	std::vector<std::string_view> PinIn::CharPool::getPinyinViewVec(size_t i, bool hasTone)const {
		//编辑i当作索引id即可
		size_t cursor = 0;//直接在result上构造字符串，用这个代表当前访问的字符串
		size_t StrStart = i;
		size_t SubCharSize = hasTone ? 0 : 1;
		std::vector<std::string_view> result;//提前构造一个字符串

		char tempChar = strs[i];//局部拷贝，避免多次访问
		while (tempChar) {//结尾符就退出
			if (tempChar == ',') {//不保存这个，压入下一个空字符串，移动cursor
				result.push_back(std::string_view(strs.data() + StrStart, i - StrStart - SubCharSize));//存储指针的只读数据
				cursor++;
				StrStart = i + 1;//记录下一个字符串的开头
			}
			i++;
			tempChar = strs[i];//自增完成后再取下一个字符
		}
		//保存最后一个
		result.push_back(std::string_view(strs.data() + StrStart, i - StrStart - SubCharSize));//存储指针的只读数据
		return result;
	}

	//PinIn类
	PinIn::PinIn(const std::string& path) {
		std::fstream fs = std::fstream(path, std::ios::in);
		if (!fs.is_open()) {//未成功打开 
			std::cerr << "file did not open successfully(StrToPinyin)\n";
			throw PinyinFileNoGet();
		}
		//开始读取
		std::string str;
		while (std::getline(fs, str)) {
			const Utf8StringView utf8str = Utf8StringView(str);//将字节流转换为utf8表示的字符串
			size_t i = 0;
			size_t size = utf8str.size();
			while (i + 1 < size && utf8str[i] != "#") {//#为注释，当i+1<size 即i已经到末尾字符的时候，还没检查到U+的结构即非法字符串，退出这一次循环
				if (utf8str[i] != "U" || utf8str[i + 1] != "+") {//判断是否合法
					i++;//不要忘记自增哦！ 卫语句减少嵌套增加可读性
					continue;
				}

				std::string key;
				i = i + 2;//往后移两位，准备开始存储数字
				while (i < size && utf8str[i] != ":") {//第一个是判空，第二个是判终点
					key += utf8str[i];
					i++;
				}
				if (i >= size) {
					break;
				}
				i++;//不要:符号
				uint8_t currentTone = 0;
				size_t pinyinId = NullPinyinId;
				//现在应该开始构造拼音表
				while (i < size && utf8str[i] != "#") {
					if (utf8str[i] == "," && pinyinId != NullPinyinId) {//这一段的时候需要存入音调再存入','
						//序列化步骤
						pool.putChar(currentTone + '0');//+48就是对应ASCII字符，ASCII字符是有序排列的
						pool.putChar(',');//存入分界符
					}
					else if (utf8str[i] != " ") {//跳过空格
						auto it = toneMap.find(utf8str[i]);
						size_t pos;
						if (it == toneMap.end()) {//没找到
							pos = pool.put(utf8str[i]);//原封不动
						}
						else {//找到了
							pos = pool.putChar(it->second.c);//替换成无声调字符
							currentTone = it->second.tone;
						}

						if (pinyinId == NullPinyinId) {//如果是默认值则赋值代表拼音id
							pinyinId = pos;
						}
					}
					i++;
				}
				if (pinyinId != NullPinyinId) {
					pool.putChar(currentTone + 48);//+48就是对应ASCII字符 追加到末尾，这是最后一个的
					pool.putEnd();//结尾分隔
					key = UnicodeToUtf8(HexStrToInt(key));
					data[key] = pinyinId;//设置
				}
				break;//退出这次循环，读取下一行
			}
		}
		//这里应该写 模糊音兼容
	}
	bool PinIn::HasPinyin(const std::string& str)const {
		return static_cast<bool>(data.count(str));
	}

	std::vector<std::string> PinIn::GetPinyinById(const size_t id, bool hasTone)const {
		if (id == NullPinyinId) {
			return {};
		}
		if (hasTone) {
			return pool.getPinyinVec(id);
		}
		else {
			return DeleteTone<std::string>(this, id);
		}
	}

	std::vector<std::string_view> PinIn::GetPinyinViewById(const size_t id, bool hasTone)const {
		if (id == NullPinyinId) {
			return {};
		}
		if (hasTone) {
			return pool.getPinyinViewVec(id);
		}
		else {
			return DeleteTone<std::string_view>(this, id);
		}
	}

	std::vector<std::string> PinIn::GetPinyin(const std::string& str, bool hasTone)const {
		auto it = data.find(str);
		if (it == data.end()) {//没数据返回由输入字符串组成的向量
			return std::vector<std::string>{str};
		}
		if (hasTone) {//如果需要音调就直接返回
			return pool.getPinyinVec(it->second);//直接返回这个方法返回的值
		}
		return DeleteTone<std::string>(this, it->second);
	}

	std::vector<std::string_view> PinIn::GetPinyinView(const std::string& str, bool hasTone)const {
		auto it = data.find(str);
		if (it == data.end()) {//没数据返回由输入字符串组成的向量
			return std::vector<std::string_view>{str};
		}
		if (hasTone) {//有声调
			return pool.getPinyinViewVec(it->second, true);//直接返回这个方法返回的值
		}
		return DeleteTone<std::string_view>(this, it->second);
	}

	std::vector<std::vector<std::string>> PinIn::GetPinyinList(const std::string& str, bool hasTone)const {
		Utf8String utf8v(str);
		std::vector<std::vector<std::string>> result;
		result.reserve(utf8v.size());
		for (size_t i = 0; i < utf8v.size(); i++) {
			result.push_back(GetPinyin(utf8v[i], hasTone));
		}
		return result;
	}

	std::vector<std::vector<std::string_view>> PinIn::GetPinyinViewList(const std::string& str, bool hasTone)const {
		Utf8String utf8v(str);
		std::vector<std::vector<std::string_view>> result;
		result.reserve(utf8v.size());
		for (size_t i = 0; i < utf8v.size(); i++) {
			result.push_back(GetPinyinView(utf8v[i], hasTone));
		}
		return result;
	}

	PinIn::Config::Config(PinIn& ctx) :ctx{ ctx }, keyboard{ ctx.keyboard } {
		//剩下构造一些浅拷贝也无影响的
		fZh2Z = ctx.fZh2Z;
		fSh2S = ctx.fSh2S;
		fCh2C = ctx.fCh2C;
		fAng2An = ctx.fAng2An;
		fIng2In = ctx.fIng2In;
		fEng2En = ctx.fEng2En;
		fU2V = ctx.fU2V;
		accelerate = ctx.accelerate;
	}

	void PinIn::Config::commit() {
		ctx.keyboard = keyboard;
		ctx.fZh2Z = fZh2Z;
		ctx.fSh2S = fSh2S;
		ctx.fCh2C = fCh2C;
		ctx.fAng2An = fAng2An;
		ctx.fIng2In = fIng2In;
		ctx.fEng2En = fEng2En;
		ctx.fU2V = fU2V;
		ctx.accelerate = accelerate;
		//需要补齐重载逻辑
		ctx.modification++;
	}

	void PinIn::Phoneme::reload() {
		if (src.empty()) {//没数据？非法的吧！，不过就直接结束了也算一种处理了
			return;
		}
		strs.clear();
		if (ctx.fCh2C && src[0] == 'c') {
			strs.push_back("ch");
			strs.push_back("c");
		}
		else if (ctx.fSh2S && src[0] == 's') {
			strs.push_back("sh");
			strs.push_back("s");
		}
		else if (ctx.fZh2Z && src[0] == 'z') {
			strs.push_back("zh");
			strs.push_back("z");
		}
		else if (ctx.fU2V && src[0] == 'v') {//我们可以做一次字符串长度检查完成是v还是ve的操作
			if (src.size() == 2) {
				strs.push_back("ue");
			}
			else {
				strs.push_back("v");
			}
		}
		else if (src.size() >= 2 && src[1] == 'n') {
			//需要有边界检查，他原本的逻辑是检查如果为ang，则添加an，反过来也一样
			//那么为什么我不直接检查到an，就两个都添加呢？反正手动插入避免查重了 下面的同理
			//还有可以提前检查n
			if (ctx.fAng2An && src[0] == 'a') {
				strs.push_back("an");
				strs.push_back("ang");
			}
			else if (ctx.fEng2En && src[0] == 'e') {
				strs.push_back("en");
				strs.push_back("eng");
			}
			else if (ctx.fIng2In && src[0] == 'i') {
				strs.push_back("in");
				strs.push_back("ing");
			}
		}

		if (strs.empty()) {
			strs.push_back(src);//都没有就把你自己插进去
		}
	}
}
