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
		try {
			return std::stoi(str, nullptr, 16);
		}
		catch (std::invalid_argument&) {//跳过错误行策略
			return -1;
		}
		catch (std::out_of_range&) {//跳过错误行策略
			return -1;
		}
	}

	bool operator==(const PinIn::Phoneme& a, const PinIn::Phoneme& b) {
		return a.GetSrc() == b.GetSrc();
	}

	size_t PinIn::CharPool::put(const std::string_view& s) {
		size_t result = strs->size();
		strs->insert(strs->end(), s.begin(), s.end());//插入字符串
		return result;
	}

	size_t PinIn::CharPool::putChar(const char s) {
		size_t result = strs->size();
		strs->push_back(s);
		return result;
	}

	void PinIn::CharPool::putEnd() {
		strs->push_back('\0');
	}

	std::vector<std::string> PinIn::CharPool::getPinyinVec(size_t i)const {//根据理论上的正确格式来讲，应当是用','字符分隔拼音，然后用'\0'作为拼音数据末尾
		//编辑i当作索引id即可
		size_t cursor = 0;//直接在result上构造字符串，用这个代表当前访问的字符串
		std::vector<std::string> result(1);//提前构造一个字符串

		char tempChar = FixedStrs[i];//局部拷贝，避免多次访问
		while (tempChar) {//结尾符就退出
			if (tempChar == ',') {//不保存这个，压入下一个空字符串，移动cursor
				result.push_back("");
				cursor++;
			}
			else {
				result[cursor].push_back(tempChar);
			}
			i++;
			tempChar = FixedStrs[i];//自增完成后再取下一个字符
		}
		return result;
	}

	std::string_view PinIn::CharPool::getPinyinView(size_t i) const {
		const size_t start = i;
		while (FixedStrs[i] != ',' && FixedStrs[i] != '\0') {
			i++;
		}
		return std::string_view(FixedStrs.get() + start, i - start);
	}

	std::vector<std::string_view> PinIn::CharPool::getPinyinViewVec(size_t i, bool hasTone)const {
		//编辑i当作索引id即可
		size_t cursor = 0;//直接在result上构造字符串，用这个代表当前访问的字符串
		size_t StrStart = i;
		size_t SubCharSize = hasTone ? 0 : 1;
		std::vector<std::string_view> result;//提前构造一个字符串

		char tempChar = FixedStrs[i];//局部拷贝，避免多次访问
		while (tempChar) {//结尾符就退出
			if (tempChar == ',') {//不保存这个，压入下一个空字符串，移动cursor
				result.push_back(std::string_view(FixedStrs.get() + StrStart, i - StrStart - SubCharSize));//存储指针的只读数据
				cursor++;
				StrStart = i + 1;//记录下一个字符串的开头
			}
			i++;
			tempChar = FixedStrs[i];//自增完成后再取下一个字符
		}
		//保存最后一个
		result.push_back(std::string_view(FixedStrs.get() + StrStart, i - StrStart - SubCharSize));//存储指针的只读数据
		return result;
	}

	void PinIn::LineParser(const std::string_view str, std::vector<InsertStrData>& InsertData) {
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
			int KeyInt = HexStrToInt(key);
			if (KeyInt == -1) {//如果捕获到异常
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
				pool.putChar(currentTone + '0');//+48就是对应ASCII字符 追加到末尾，这是最后一个的
				pool.putEnd();//结尾分隔
				key = UnicodeToUtf8(KeyInt);
				size_t keyId = pool.put(key);
				InsertData.push_back({ key.size(),keyId,pinyinId });
				//data.insert_or_assign(key, pinyinId);//设置
			}
			break;//退出这次循环，读取下一行
		}
	}

	void PinIn::InsertDataFn(std::vector<InsertStrData>& InsertData) {
		char* poolptr = pool.data();
		for (const auto& v : InsertData) {//因为内存池是只读设计，所以构造完成内存池后，可以将字符串视图当作key，降低对象内存开销
			data.insert_or_assign(std::string_view(poolptr + v.keyStart, v.keySize), v.valueId);
		}
	}

	//PinIn类
	PinIn::PinIn(const std::string& path) {
		std::fstream fs = std::fstream(path, std::ios::in);
		if (!fs.is_open()) {//未成功打开 
			//std::cerr << "file did not open successfully(StrToPinyin)\n";
			throw PinyinFileNotOpen();
		}
		//开始读取
		std::string str;
		std::vector<InsertStrData> cache;
		while (std::getline(fs, str)) {
			LineParser(str, cache);
		}
		pool.Fixed();
		InsertDataFn(cache);
	}

	PinIn::PinIn(const std::vector<char>& input_data) {
		//开始读取
		std::string_view str;
		std::vector<InsertStrData> cache;
		size_t last_cursor = 0;
		for (size_t i = 0; i < input_data.size(); i++) {
			if (input_data[i] == '\n') {//按行解析
				LineParser(std::string_view(input_data.data() + last_cursor, i - last_cursor), cache);
				last_cursor = i + 1;//跳过换行
			}
		}
		LineParser(std::string_view(input_data.data() + last_cursor, input_data.size() - last_cursor), cache);//解析最后一行
		pool.Fixed();
		InsertDataFn(cache);
	}

	bool PinIn::HasPinyin(const std::string_view& str)const {
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

	std::vector<std::string> PinIn::GetPinyin(const std::string_view& str, bool hasTone)const {
		auto it = data.find(str);
		if (it == data.end()) {//没数据返回由输入字符串组成的向量
			return std::vector<std::string>{std::string(str)};
		}
		if (hasTone) {//如果需要音调就直接返回
			return pool.getPinyinVec(it->second);//直接返回这个方法返回的值
		}
		return DeleteTone<std::string>(this, it->second);
	}

	std::vector<std::string_view> PinIn::GetPinyinView(const std::string_view& str, bool hasTone)const {
		auto it = data.find(str);
		if (it == data.end()) {//没数据返回由输入字符串组成的向量
			return std::vector<std::string_view>{str};
		}
		if (hasTone) {//有声调
			return pool.getPinyinViewVec(it->second, true);//直接返回这个方法返回的值
		}
		return DeleteTone<std::string_view>(this, it->second);
	}

	std::vector<std::vector<std::string>> PinIn::GetPinyinList(const std::string_view& str, bool hasTone)const {
		Utf8StringView utf8v(str);
		std::vector<std::vector<std::string>> result;
		result.reserve(utf8v.size());
		for (size_t i = 0; i < utf8v.size(); i++) {
			result.push_back(GetPinyin(utf8v[i], hasTone));
		}
		return result;
	}

	std::vector<std::vector<std::string_view>> PinIn::GetPinyinViewList(const std::string_view& str, bool hasTone)const {
		Utf8StringView utf8v(str);
		std::vector<std::vector<std::string_view>> result;
		result.reserve(utf8v.size());
		for (size_t i = 0; i < utf8v.size(); i++) {
			result.push_back(GetPinyinView(utf8v[i], hasTone));
		}
		return result;
	}

	PinIn::Character PinIn::GetChar(const std::string_view& str) {
		return Character(*this, str, GetPinyinId(str));
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
		ctx.fFirstChar = fFirstChar;

		if (ctx.CharCache) {
			for (const auto& v : ctx.CharCache.value()) {
				v.second->reload();
			}
		}

		ctx.modification++;
	}

	static size_t StrCmp(const Utf8String& a, const Utf8StringView& b, size_t aStart) {//实际上只有一个函数在用，为了它改造一下也没啥问题
		size_t len = std::min(a.size() - aStart, b.size());
		for (size_t i = 0; i < len; i++) {
			if (a[i + aStart] != b[i]) {
				return i;
			}
		}
		return len;
	}

	bool PinIn::Phoneme::matchSequence(const char c)const {
		for (const auto& str : strs) {
			if (str[0] == c) {
				return true;
			}
		}
		return false;
	}

	IndexSet PinIn::Phoneme::match(const Utf8String& source, IndexSet idx, size_t start, bool partial)const {
		if (empty()) {
			return idx;
		}
		IndexSet result = IndexSet::GetNone();
		idx.foreach([&](uint32_t i) {//&为捕获所有参数的引用，因为在函数内，所以是安全的，他不是注册一个异步操作
			IndexSet is = match(source, start + i, partial);
			is.offset(i);
			result.merge(is);
		});

		return result;
	}

	IndexSet PinIn::Phoneme::match(const Utf8String& source, size_t start, bool partial)const {
		IndexSet result = IndexSet::GetNone();
		if (empty()) {
			return result;
		}
		for (const auto& str : strs) {
			size_t size = StrCmp(source, str, start);
			if (partial && start + size == source.size()) {//显式手动转换，表明我知道这个转换且需要，避免编译期警告
				result.set(static_cast<uint32_t>(size));  // ending match
			}
			else if (size == str.size()) {
				result.set(static_cast<uint32_t>(size)); // full match
			}
		}
		return result;
	}

	void PinIn::Phoneme::reloadNoMap() {
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
				strs.push_back("ve");

				if (ctx.fFirstChar) {//如果开了这个，那么就同时加入
					strs.push_back("u");
					strs.push_back("v");
				}
			}
			else {
				strs.push_back("u");
				strs.push_back("v");
			}
		}
		else {//分支，即都没有增加第一个字符的情况
			if (ctx.fFirstChar) {
				strs.push_back(src.substr(0, 1));
			}
			if (src.size() >= 2 && src[1] == 'n') {
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
		}

		if (strs.empty() || (ctx.fFirstChar && src.size() > 1)) {//没有，或者首字母模式时字符串长度大于1插入自己
			strs.push_back(src);
		}

		for (auto& str : strs) {
			str = ctx.keyboard.keys(str);//处理映射逻辑
		}
	}
	void PinIn::Phoneme::reloadHasMap() {
		//这次需要查重了
		std::set<std::string_view> StrSet;
		StrSet.insert(src);
		if (ctx.fFirstChar && src.size() > 1) {
			StrSet.insert(src.substr(0, 1));
		}
		if (ctx.fCh2C && src[0] == 'c') {//最简单的几个
			StrSet.insert("ch");
			StrSet.insert("c");
		}
		if (ctx.fSh2S && src[0] == 's') {
			StrSet.insert("sh");
			StrSet.insert("s");
		}
		if (ctx.fZh2Z && src[0] == 'z') {
			StrSet.insert("zh");
			StrSet.insert("z");
		}
		//将匹配逻辑内聚
		if (ctx.fU2V && src[0] == 'v'//简单的检查字符串可以避免内部查表
			|| (ctx.fAng2An && src.ends_with("ang"))
			|| (ctx.fEng2En && src.ends_with("eng"))
			|| (ctx.fIng2In && src.ends_with("ing"))
			|| (ctx.fAng2An && src.ends_with("an"))
			|| (ctx.fEng2En && src.ends_with("en"))
			|| (ctx.fIng2In && src.ends_with("in"))) {
			for (const auto& str : ctx.keyboard.GetFuzzyPhoneme(src)) {
				StrSet.insert(str);
			}
		}

		for (const auto& str : StrSet) {
			strs.push_back(ctx.keyboard.keys(str));//将视图压入向量
		}
	}

	void PinIn::Phoneme::reload() {
		strs.clear();//应该前置，因为是在重载，非法的话就当然置空了
		if (src.empty()) {//没数据？非法的吧！，不过就直接结束了也算一种处理了
			return;
		}
		if (src.size() == 1 && src[0] >= '0' && src[0] <= '4') {
			strs.push_back(src); //声调就是它自己，直接处理完毕返回！
			return;
		}
		if (ctx.keyboard.GetHasFuuzyLocal()) {
			reloadHasMap();//非标准音素，部分纯逻辑加查表实现
		}
		else {
			reloadNoMap();//标准音素，纯逻辑实现
		}
	}

	void PinIn::Pinyin::reload() {
		std::string_view str = ctx.pool.getPinyinView(id);//临时获取视图
		duo = ctx.keyboard.duo;
		sequence = ctx.keyboard.sequence;
		phonemes.clear();//清空
		for (const auto& str : ctx.keyboard.split(str)) {
			phonemes.push_back(Phoneme(ctx, str));//构建音素后缓存进去
		}
	}

	IndexSet PinIn::Pinyin::match(const Utf8String& str, size_t start, bool partial)const {
		IndexSet ret = IndexSet::GetNone();
		if (duo) {
			// in shuangpin we require initial and final both present,
			// the phoneme, which is tone here, is optional
			ret = IndexSet::ZERO;
			ret = phonemes[0].match(str, ret, start, partial);
			ret = phonemes[1].match(str, ret, start, partial);
			ret.merge(phonemes[2].match(str, ret, start, partial));
		}
		else {
			// in other keyboards, match of precedent phoneme
			// is compulsory to match subsequent phonemes
			// for example, zhong1, z+h+ong+1 cannot match zong or zh1
			IndexSet active = IndexSet::ZERO;
			for (const Phoneme& phoneme : phonemes) {
				active = phoneme.match(str, active, start, partial);
				if (active.empty()) break;
				ret.merge(active);
			}
		}
		if (sequence && phonemes[0].matchSequence(str[start][0])) {//内部音素都是ASCII范围内的，所以本质上就是在比较ASCII，直接取字符丢进去比较就行
			ret.set(1);
		}

		return ret;
	}

	PinIn::Character::Character(const PinIn& p, const std::string_view& ch, const size_t id) :ctx{ p }, id{ id }, ch{ ch } {
		if (id == NullPinyinId) {
			return;//无效拼音数据
		}
		size_t currentId = id;
		for (const auto& str : p.GetPinyinViewById(id, true)) {//split需要处理带声调的版本
			pinyin.push_back(Pinyin(ctx, currentId));
			currentId += str.size() + 2;//因为有个分隔符和声调，所以要+2要跳过直到下一个字符串起始
		}
	}

	IndexSet PinIn::Character::match(const Utf8String& u8str, size_t start, bool partial)const {
		IndexSet ret = (u8str[start] == ch ? IndexSet::ONE : IndexSet::NONE).copy();
		for (const auto& p : pinyin) {
			ret.merge(p.match(u8str, start, partial));
		}
		return ret;
	}
}
