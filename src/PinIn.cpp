#include "PinIn4Cpp/PinIn.h"

#include "PinIn4Cpp/detail/BinUtils.h"

namespace PinInCpp {
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
				result.emplace_back(std::string_view(FixedStrs.get() + StrStart, i - StrStart - SubCharSize));//存储指针的只读数据
				cursor++;
				StrStart = i + 1;//记录下一个字符串的开头
			}
			i++;
			tempChar = FixedStrs[i];//自增完成后再取下一个字符
		}
		//保存最后一个
		result.emplace_back(std::string_view(FixedStrs.get() + StrStart, i - StrStart - SubCharSize));//存储指针的只读数据
		return result;
	}

	void PinIn::LineParser(const detail::Utf8StringView& utf8str) {
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
			int KeyInt = detail::HexStrToInt(key);
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
				data.insert_or_assign(detail::UnicodeToUtf8(KeyInt), pinyinId);//设置
			}
			break;//退出这次循环，读取下一行
		}
	}

	//PinIn类
	PinIn::PinIn(std::string_view path) {
		std::fstream fs = std::fstream(std::string(path), std::ios::in);
		if (!fs.is_open()) {//未成功打开 
			//std::cerr << "file did not open successfully(StrToPinyin)\n";
			throw PinyinFileNotOpenException();
		}
		//开始读取
		std::string str;
		detail::Utf8StringView utf8str;
		while (std::getline(fs, str)) {
			utf8str.reset(str);
			LineParser(utf8str);
		}
		pool.Fixed();
	}

	PinIn::PinIn(const std::vector<char>& input_data) {
		//开始读取
		std::string_view str;
		size_t last_cursor = 0;
		detail::Utf8StringView utf8str;
		for (size_t i = 0; i < input_data.size(); i++) {
			if (input_data[i] == '\n') {//按行解析
				utf8str.reset(std::string_view(input_data.data() + last_cursor, i - last_cursor));
				LineParser(utf8str);
				last_cursor = i + 1;//跳过换行
			}
		}
		utf8str.reset(std::string_view(input_data.data() + last_cursor, input_data.size() - last_cursor));
		LineParser(utf8str);//解析最后一行
		pool.Fixed();
	}

	bool PinIn::HasPinyin(std::string_view str)const noexcept {
		return static_cast<bool>(data.count(detail::FourCCToU32(str)));
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
			return pool.getPinyinViewVec(id, true);
		}
		else {
			return DeleteTone<std::string_view>(this, id);
		}
	}

	std::vector<std::string> PinIn::GetPinyin(std::string_view str, bool hasTone)const {
		auto it = data.find(detail::FourCCToU32(str));
		if (it == data.end()) {//没数据返回由输入字符串组成的向量
			return std::vector<std::string>{std::string(str)};
		}
		if (hasTone) {//如果需要音调就直接返回
			return pool.getPinyinVec(it->second);//直接返回这个方法返回的值
		}
		return DeleteTone<std::string>(this, it->second);
	}

	std::vector<std::string_view> PinIn::GetPinyinView(std::string_view str, bool hasTone)const {
		auto it = data.find(detail::FourCCToU32(str));
		if (it == data.end()) {//没数据返回由输入字符串组成的向量
			return std::vector<std::string_view>{str};
		}
		if (hasTone) {//有声调
			return pool.getPinyinViewVec(it->second, true);//直接返回这个方法返回的值
		}
		return DeleteTone<std::string_view>(this, it->second);
	}

	std::vector<std::vector<std::string>> PinIn::GetPinyinList(std::string_view str, bool hasTone)const {
		detail::Utf8StringView utf8v(str);
		std::vector<std::vector<std::string>> result;
		result.reserve(utf8v.size());
		for (size_t i = 0; i < utf8v.size(); i++) {
			result.emplace_back(GetPinyin(utf8v[i], hasTone));
		}
		return result;
	}

	std::vector<std::vector<std::string_view>> PinIn::GetPinyinViewList(std::string_view str, bool hasTone)const {
		detail::Utf8StringView utf8v(str);
		std::vector<std::vector<std::string_view>> result;
		result.reserve(utf8v.size());
		for (size_t i = 0; i < utf8v.size(); i++) {
			result.emplace_back(GetPinyinView(utf8v[i], hasTone));
		}
		return result;
	}

	PinIn::Character* PinIn::GetCharCachePtr(std::string_view str) {
		if (CharCache) {
			size_t id = GetPinyinId(str);
			std::unordered_map<size_t, std::unique_ptr<Character>>& cache = CharCache.value();
			auto it = cache.find(id);
			if (it == cache.end()) {//缓存不存在时
				std::unique_ptr<Character> ptr = std::unique_ptr<Character>(new Character(*this, str, id));
				Character* result = ptr.get();
				cache.insert_or_assign(id, std::move(ptr));
				return result;
			}
			else {
				return it->second.get();//缓存存在时
			}
		}
		else {
			return nullptr;
		}
	}

	PinIn::Character* PinIn::GetCharCachePtr(const uint32_t fourCC) {
		if (CharCache) {
			size_t id = GetPinyinId(fourCC);
			std::unordered_map<size_t, std::unique_ptr<Character>>& cache = CharCache.value();
			auto it = cache.find(id);
			if (it == cache.end()) {//缓存不存在时
				char buf[5];
				detail::U32FourCCToCharBuf(buf, fourCC);
				std::unique_ptr<Character> ptr = std::unique_ptr<Character>(new Character(*this, buf, id));
				Character* result = ptr.get();
				cache.insert_or_assign(id, std::move(ptr));
				return result;
			}
			else {
				return it->second.get();//缓存存在时
			}
		}
		else {
			return nullptr;
		}
	}

	void PinIn::PreCacheString(std::string_view str) {
		if (!CharCache) {
			return;
		}
		std::unordered_map<size_t, std::unique_ptr<Character>>& cache = CharCache.value();
		size_t cursor = 0;
		size_t end = str.size();
		char buf[5];//缓冲区，避免堆分配
		while (cursor < end) {
			size_t charSize = detail::getUTF8CharSize(str[cursor]);
			for (size_t i = 0; i < charSize; i++) {//根据获取长度，深拷贝数据
				buf[i] = str[cursor + i];
			}
			buf[charSize] = '\0';//加终止符

			size_t id = GetPinyinId(buf);
			if (id != NullPinyinId && !cache.count(id)) {
				cache.insert_or_assign(id, std::unique_ptr<Character>(new Character(*this, buf, id)));
			}
			cursor += charSize;
		}
	}

	PinIn::Config::Config(PinIn& ctx) : keyboard{ ctx.keyboard }, ctx{ ctx } {
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

		ctx.PinyinTotals = 0;
		ctx.phonemes.clear();
		ctx.pinyins.clear();
		if (ctx.CharCache) {
			for (const auto& v : ctx.CharCache.value()) {
				v.second->reload();
			}
		}

		ctx.modification++;
	}

	std::shared_ptr<PinIn> PinIn::Deserialize(const std::vector<uint8_t>& data, const std::optional<Keyboard>& keyboard, size_t index) {
		detail::VecU8Reader reader(data, index);
		uint32_t ver = reader.GetDoubleWord();
		if (ver != BinDataVersion) {
			throw BinaryVersionInvalidException("PinIn: Invalid binary file version");
		}
		std::shared_ptr<PinIn> result(new PinIn);
		if (keyboard.has_value()) {//keyboard注入
			result->keyboard = keyboard.value();
		}
		//配置加载
		result->fZh2Z = reader.GetByte();
		result->fSh2S = reader.GetByte();
		result->fCh2C = reader.GetByte();
		result->fAng2An = reader.GetByte();
		result->fIng2In = reader.GetByte();
		result->fEng2En = reader.GetByte();
		result->fU2V = reader.GetByte();
		result->fFirstChar = reader.GetByte();
		bool CharCacheEnable = reader.GetByte();
		if (!CharCacheEnable) {
			result->SetCharCache(false);
		}

		size_t poolSize = reader.GetSizeTFromQW();

		std::unique_ptr<char[]> poolData = std::unique_ptr<char[]>(new char[poolSize]);//重建字符数据
		memcpy(poolData.get(), data.data() + reader.GetIndex(), poolSize);
		result->pool = CharPool(std::move(poolData), poolSize);
		reader.AddIndex(poolSize);

		size_t dataSize = reader.GetSizeTFromQW();

		for (size_t i = 0; i < dataSize; i++) {//重建关联数据
			uint32_t k = reader.GetDoubleWord();
			size_t v = reader.GetSizeTFromQW();
			result->data.insert_or_assign(k, v);
		}

		if (CharCacheEnable) {
			result->PreNullPinyinIdCache();//不管怎么样，插入这个都是好的
			size_t cacheSize = reader.GetSizeTFromQW();

			for (size_t i = 0; i < cacheSize; i++) {
				uint32_t fourCC = reader.GetDoubleWord();
				result->GetCharCachePtr(fourCC);
			}
		}

		return result;
	}

	std::vector<uint8_t> PinIn::Serialize()const {
		std::vector<uint8_t> result;
		detail::PushDWUint8(result, BinDataVersion);

		result.push_back(fZh2Z);
		result.push_back(fSh2S);
		result.push_back(fCh2C);
		result.push_back(fAng2An);
		result.push_back(fIng2In);
		result.push_back(fEng2En);
		result.push_back(fU2V);
		result.push_back(fFirstChar);
		result.push_back(CharCache.has_value());//插入是否开启缓存的数据

		detail::PushQWUint8(result, pool.size());//字符数组数据
		if (empty()) {
			return result;
		}
		result.insert(result.end(), pool.data(), pool.data() + pool.size());

		detail::PushQWUint8(result, data.size());//关联表数据
		for (const auto& [k, v] : data) {
			detail::PushDWUint8(result, k);
			detail::PushQWUint8(result, v);
		}

		if (CharCache.has_value()) {
			size_t CacheSize = CharCache.value().size();
			if (CharCache.value().count(NullPinyinId)) {//万恶的差一错误
				CacheSize--;
			}
			detail::PushQWUint8(result, CacheSize);
			for (const auto& v : CharCache.value()) {
				if (v.second->id == NullPinyinId) {
					continue;
				}
				detail::PushDWUint8(result, detail::FourCCToU32(v.second->ch));//用FourCC方便序列化时重建数据
			}
		}
		return result;
	}

	std::vector<uint8_t> PinIn::PreCacheSerialize()const {
		std::vector<uint8_t> result;
		if (CharCache.has_value()) {
			size_t CacheSize = CharCache.value().size();
			if (CharCache.value().count(NullPinyinId)) {//万恶的差一错误
				CacheSize--;
			}
			detail::PushQWUint8(result, CacheSize);
			for (const auto& v : CharCache.value()) {
				if (v.second->id == NullPinyinId) {
					continue;
				}
				detail::PushDWUint8(result, detail::FourCCToU32(v.second->ch));//用FourCC方便序列化时重建数据
			}
		}
		return result;
	}

	void PinIn::PreCacheDeserialize(const std::vector<uint8_t>& data, size_t index) {
		detail::VecU8Reader reader(data, index);
		PreNullPinyinIdCache();//不管怎么样，插入这个都是好的
		size_t cacheSize = reader.GetSizeTFromQW();

		for (size_t i = 0; i < cacheSize; i++) {
			uint32_t fourCC = reader.GetDoubleWord();
			GetCharCachePtr(fourCC);
		}
	}

	inline static size_t StrCmp(const detail::Utf8StringView& a, const detail::Utf8StringView& b, size_t aStart) {//实际上只有一个函数在用，为了它改造一下也没啥问题
		size_t len = std::min(a.size() - aStart, b.size());
		for (size_t i = 0; i < len; i++) {
			if (a[i + aStart] != b[i]) {
				return i;
			}
		}
		return len;
	}

	bool PinIn::Phoneme::matchSequence(const char c)const noexcept {
		for (const auto& str : strs) {
			if (str[0] == c) {
				return true;
			}
		}
		return false;
	}

	detail::IndexSet PinIn::Phoneme::match(const detail::Utf8StringView& source, detail::IndexSet idx, size_t start, bool partial)const noexcept {
		if (empty()) {
			return idx;
		}
		detail::IndexSet result = detail::IndexSet::Init();

		detail::IndexSet::IndexSetIterObj it = idx.GetIterObj();
		for (uint32_t i = it.Next(); i != it.end(); i = it.Next()) {
			detail::IndexSet is = match(source, start + i, partial);
			is.offset(i);
			result.merge(is);
		}
		return result;
	}

	detail::IndexSet PinIn::Phoneme::match(const detail::Utf8StringView& source, size_t start, bool partial)const noexcept {
		detail::IndexSet result = detail::IndexSet::Init();
		if (empty()) {
			return result;
		}
		for (const auto& str : strs) {
			size_t size = StrCmp(source, str, start);
			if (size == 0) {
				continue;
			}
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
			strs.emplace_back("ch");
			strs.emplace_back("c");
		}
		else if (ctx.fSh2S && src[0] == 's') {
			strs.emplace_back("sh");
			strs.emplace_back("s");
		}
		else if (ctx.fZh2Z && src[0] == 'z') {
			strs.emplace_back("zh");
			strs.emplace_back("z");
		}
		else if (ctx.fU2V && src[0] == 'v') {//我们可以做一次字符串长度检查完成是v还是ve的操作
			if (src.size() == 2) {
				strs.emplace_back("ue");
				strs.emplace_back("ve");

				if (ctx.fFirstChar) {//如果开了这个，那么就同时加入
					strs.emplace_back("u");
					strs.emplace_back("v");
				}
			}
			else {
				strs.emplace_back("u");
				strs.emplace_back("v");
			}
		}
		else {//分支，即都没有增加第一个字符的情况
			if (ctx.fFirstChar) {
				strs.emplace_back(src.substr(0, 1));
			}
			if (src.size() >= 2 && src[1] == 'n') {
				//需要有边界检查，他原本的逻辑是检查如果为ang，则添加an，反过来也一样
				//那么为什么我不直接检查到an，就两个都添加呢？反正手动插入避免查重了 下面的同理
				//还有可以提前检查n
				if (ctx.fAng2An && src[0] == 'a') {
					strs.emplace_back("an");
					strs.emplace_back("ang");
				}
				else if (ctx.fEng2En && src[0] == 'e') {
					strs.emplace_back("en");
					strs.emplace_back("eng");
				}
				else if (ctx.fIng2In && src[0] == 'i') {
					strs.emplace_back("in");
					strs.emplace_back("ing");
				}
			}
		}

		if (strs.empty() || (ctx.fFirstChar && src.size() > 1)) {//没有，或者首字母模式时字符串长度大于1插入自己
			strs.emplace_back(src);
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
		if ((ctx.fU2V && src[0] == 'v')//简单的检查字符串可以避免内部查表
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
			strs.emplace_back(ctx.keyboard.keys(str));//将视图压入向量
		}
	}

	void PinIn::Phoneme::reload() {
		strs.clear();//应该前置，因为是在重载，非法的话就当然置空了
		if (src.empty()) {//没数据？非法的吧！，不过就直接结束了也算一种处理了
			return;
		}
		if (src.size() == 1 && src[0] >= '0' && src[0] <= '4') {
			strs.emplace_back(src); //声调就是它自己，直接处理完毕返回！
			return;
		}
		if (ctx.keyboard.GetHasFuuzyLocal()) {
			reloadHasMap();//非标准音素，部分纯逻辑加查表实现
		}
		else {
			reloadNoMap();//标准音素，纯逻辑实现
		}
	}

	void PinIn::Pinyin::reload(std::string_view str) {
		duo = ctx.keyboard.duo;
		sequence = ctx.keyboard.sequence;
		phonemes.clear();//清空
		for (const auto& SrcPh : ctx.keyboard.split(str)) {
			auto it = ctx.phonemes.find(SrcPh);
			if (it != ctx.phonemes.end()) {
				phonemes.emplace_back(it->second.get());
			}
			else {
				Phoneme* ph = ctx.phonemes.insert_or_assign(SrcPh, std::unique_ptr<Phoneme>(new Phoneme(ctx, SrcPh))).first->second.get();//构建音素后缓存进去
				phonemes.emplace_back(ph);
			}
		}
	}

	detail::IndexSet PinIn::Pinyin::match(const detail::Utf8StringView& str, size_t start, bool partial)const noexcept {
		detail::IndexSet ret = detail::IndexSet::Init();
		if (duo) {
			// in shuangpin we require initial and final both present,
			// the phoneme, which is tone here, is optional
			ret = detail::IndexSet::ZERO;
			ret = phonemes[0]->match(str, ret, start, partial);
			ret = phonemes[1]->match(str, ret, start, partial);
			ret.merge(phonemes[2]->match(str, ret, start, partial));
		}
		else {
			// in other keyboards, match of precedent phoneme
			// is compulsory to match subsequent phonemes
			// for example, zhong1, z+h+ong+1 cannot match zong or zh1
			detail::IndexSet active = detail::IndexSet::ZERO;
			for (const Phoneme* phoneme : phonemes) {
				active = phoneme->match(str, active, start, partial);
				if (active.empty()) break;
				ret.merge(active);
			}
		}
		//内部音素都是ASCII范围内的，所以本质上就是在比较ASCII，直接取字符丢进去比较就行
		//UTF8的ASCII字符和原始的ASCII字符一致，我们检查一下大小可以作为快路径跳出，如果不符合要求的话
		if (sequence && str[start].size() == 1 && phonemes[0]->matchSequence(str[start][0])) {
			ret.set(1);
		}

		return ret;
	}

	void PinIn::Character::reload() {
		if (id == NullPinyinId) {
			return;//无效拼音数据
		}
		pinyin.clear();
		for (const auto& str : ctx.GetPinyinViewById(id, true)) {//split需要处理带声调的版本
			auto it = ctx.pinyins.find(str);
			if (it != ctx.pinyins.end()) {
				pinyin.emplace_back(it->second.get());
			}
			else {
				Pinyin* newData = ctx.pinyins.insert_or_assign(str, std::unique_ptr<Pinyin>(new Pinyin(ctx, ctx.PinyinTotals, str))).first->second.get();
				ctx.PinyinTotals++;
				pinyin.emplace_back(newData);
			}
		}
	}

	detail::IndexSet PinIn::Character::match(const detail::Utf8StringView& u8str, size_t start, bool partial)const noexcept {
		detail::IndexSet ret = u8str[start] == ch ? detail::IndexSet::ONE : detail::IndexSet::NONE;
		for (const auto& p : pinyin) {
			ret.merge(p->match(u8str, start, partial));
		}
		return ret;
	}
}
