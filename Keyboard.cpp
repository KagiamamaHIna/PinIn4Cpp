#include "Keyboard.h"

namespace PinInCpp {
	bool hasInitial(const std::string_view& s) {//判断是否有声母
		if (s.empty()) {
			return false;
		}
		//检查第一个字符
		switch (s.front()) {
		case 'a':
		case 'e':
		case 'i':
		case 'o':
		case 'u':
		case 'v': //'v' 代表 'ü'
			return false; //如果是元音开头，说明没有（辅音）声母
		default:
			return true;  //其他所有情况（辅音开头），说明有声母
		}
	}

	void Keyboard::InsertDataFn(const OptionalStrMap& srcData, std::vector<InsertStrData>& data, std::vector<InsertStrMultiData>* LocalFuzzyData) {
		if (LocalFuzzyData) {//有的话，循环的同时需要进行动态模糊音匹配
			std::set<std::string_view> HasData;
			for (const auto& [key, value] : srcData.value()) {
				size_t keySize = key.size();
				size_t keyStart = pool.put(key);

				size_t valueSize = value.size();
				size_t valueStart = pool.put(value);
				data.push_back({ keySize,keyStart,valueSize,valueStart });
				for (const auto& str : Keyboard::standard(value)) {//key是基于标准拼音的，所以只用检查value
					if (HasData.count(str)) {//有就直接跳过
						continue;
					}
					//没有就先插入，避免重复判断
					HasData.insert(str);
					InsertStrMultiData data = { std::string::npos ,std::string::npos ,{} };

					//应该是匹配对数组，要稍作改造，比如van这样的情况，可以同时有uan和uang的规则
					if (str[0] == 'v') {//如果开头是v，那么就是要对应匹配的
						data.keySize = str.size();
						data.keyStart = pool.put(str);//插入原始匹配的

						size_t valueStart = pool.putChar('u');
						size_t valueSize = keySize;//本质上相当于一次文本替换，那长度当然不变
						data.values.push_back({ valueSize , valueStart });
						pool.putCharPtr(str.data() + 1, keySize - 1);//偏移一位，然后记录长度减一就行
					}
					if (str.ends_with("ang") || str.ends_with("eng") || str.ends_with("ing")) {//这个规则之间是互斥的，所以继续if else结构
						if (data.keySize == std::string::npos) {//检查是否被初始化
							data.keySize = str.size();
							data.keyStart = pool.put(str);//插入原始匹配的
						}
						data.values.push_back({ data.keySize - 1 , data.keyStart });//本质上相当于一次字符串裁剪，只是剪掉了g，所以-1，且可以复用
					}
					else if (str.ends_with("an") || str.ends_with("en") || str.ends_with("in")) {
						if (data.keySize == std::string::npos) {//检查是否被初始化
							data.keySize = str.size();
							data.keyStart = pool.put(str);//插入原始匹配的
						}
						data.values.push_back({ data.keySize + 1 , data.keyStart });//本质上相当于一次字符串裁剪，只是剪掉了g，所以-1，且可以复用
						pool.putChar('g');//插入一个g进去
					}
					if (data.keySize != std::string::npos) {//既然不是无效宽度了，那么就代表检查成功了，即符合某一模糊音匹配规则
						LocalFuzzyData->push_back(data);
					}
				}
			}
		}
		else {
			for (const auto& [key, value] : srcData.value()) {
				size_t keySize = key.size();
				size_t keyStart = pool.put(key);

				size_t valueSize = value.size();
				size_t valueStart = pool.put(value);
				data.push_back({ keySize,keyStart,valueSize,valueStart });
			}
		}
	}
	void Keyboard::CreateViewOnMap(std::map<std::string_view, std::string_view>& Target, const std::vector<InsertStrData>& data) {
		char* poolptr = pool.data();
		for (const auto& data : data) {
			std::string_view key(poolptr + data.keyStart, data.keySize);
			std::string_view value(poolptr + data.valueStart, data.valueSize);
			Target.insert_or_assign(key, value);
		}
	}
	void Keyboard::ViewDeepCopy(const Keyboard::StrPool& srcPool, const std::map<std::string_view, std::string_view>& srcMap, std::map<std::string_view, std::string_view>& Target) {
		size_t srcPoolPtr = reinterpret_cast<size_t>(srcPool.data());
		char* poolptr = pool.data();
		for (const auto& [k, v] : srcMap) {
			size_t keyStart = reinterpret_cast<size_t>(k.data());
			keyStart -= srcPoolPtr;//视图指针基址减去其来源即他的偏移量

			size_t valueStart = reinterpret_cast<size_t>(v.data());
			valueStart -= srcPoolPtr;//视图指针基址减去其来源即他的偏移量

			std::string_view key(poolptr + keyStart, k.size());
			std::string_view value(poolptr + valueStart, v.size());
			Target.insert_or_assign(key, value);
		}
	}
	Keyboard::Keyboard(const OptionalStrMap& MapLocalArg, const OptionalStrMap& MapKeysArg, CutterFn cutter, bool duo, bool sequence)
		:cutter{ cutter }, duo{ duo }, sequence{ sequence } {
		//在插入完成数据之前，构建视图都是不安全的行为，因为容器可能会随时扩容
		//所以需要缓存数据，在插入完成后再根据数据构建视图
		std::vector<InsertStrData> MapLocalData;
		std::vector<InsertStrMultiData> FuzzyPhoneme;
		std::vector<InsertStrData> MapKeysData;
		if (MapLocalArg != std::nullopt) {//不为空
			InsertDataFn(MapLocalArg, MapLocalData, &FuzzyPhoneme);
		}
		if (MapKeysArg != std::nullopt) {
			InsertDataFn(MapKeysArg, MapKeysData);
		}
		//数据均插入完成了，可以开始构建视图了
		if (!MapLocalData.empty()) {//检查容器是否为空，防止map里也是空的情况下构造了map，因为那样是无用的
			MapLocal = std::map<std::string_view, std::string_view>();//构建！
			CreateViewOnMap(MapLocal.value(), MapLocalData);
		}
		if (!FuzzyPhoneme.empty()) {
			MapLocalFuzzy = std::map<std::string_view, std::vector<std::string_view>>();
			auto& Target = MapLocalFuzzy.value();//jb的太长了我受不了了
			char* poolptr = pool.data();
			for (const auto& data : FuzzyPhoneme) {
				std::string_view key(poolptr + data.keyStart, data.keySize);
				for (const auto& item : data.values) {//将数据集插入
					std::string_view str(poolptr + item.valueStart, item.valueSize);
					Target[key].push_back(str);//会触发默认构造，所以不用显式赋值
				}
			}
		}
		if (!MapKeysData.empty()) {
			MapKeys = std::map<std::string_view, std::string_view>();
			CreateViewOnMap(MapKeys.value(), MapKeysData);
		}
	}
	Keyboard::Keyboard(const Keyboard& src) :duo{ src.duo }, sequence{ src.sequence }, pool{ src.pool }, cutter{ src.cutter } {
		//重建视图
		if (src.MapLocalFuzzy.has_value()) {
			MapLocalFuzzy = std::map<std::string_view, std::vector<std::string_view>>();
			char* poolptr = pool.data();
			auto& Target = MapLocalFuzzy.value();

			size_t srcPoolPtr = reinterpret_cast<size_t>(src.pool.data());
			for (const auto& [key, vec] : src.MapLocalFuzzy.value()) {
				size_t keyStart = reinterpret_cast<size_t>(key.data());
				keyStart -= srcPoolPtr;//视图指针基址减去其来源即他的偏移量

				std::string_view keyView(poolptr + keyStart, key.size());//构造键
				for (const auto& v : vec) {
					size_t valueStart = reinterpret_cast<size_t>(v.data());
					valueStart -= srcPoolPtr;//视图指针基址减去其来源即他的偏移量

					Target[keyView].push_back(std::string_view(poolptr + valueStart, v.size()));
				}
			}
		}
		if (src.MapKeys.has_value()) {
			MapKeys = std::map<std::string_view, std::string_view>();
			ViewDeepCopy(src.pool, src.MapKeys.value(), MapKeys.value());
		}
		if (src.MapLocal.has_value()) {
			MapLocal = std::map<std::string_view, std::string_view>();
			ViewDeepCopy(src.pool, src.MapLocal.value(), MapLocal.value());
		}
	}

	std::string_view Keyboard::keys(const std::string_view& s)const {
		if (MapKeys == std::nullopt) {
			return s;
		}
		const std::map<std::string_view, std::string_view>& Keys = MapKeys.value();
		auto it = Keys.find(s);
		//指向结尾为未找到
		if (it != Keys.end()) {
			return it->second;//通过迭代器直接获取值，无需再次查找
		}

		return s;
	}

	std::vector<std::string_view> Keyboard::GetFuzzyPhoneme(const std::string_view& s)const {
		if (MapLocalFuzzy == std::nullopt) {
			return { s };
		}
		const auto& Keys = MapLocalFuzzy.value();
		auto it = Keys.find(s);
		//指向结尾为未找到
		if (it != Keys.end()) {
			return it->second;//通过迭代器直接获取值，无需再次查找
		}

		return { s };
	}

	std::vector<std::string_view> Keyboard::split(const std::string_view& s)const {
		if (s.empty()) { //可选？ 要为了性能不检查吧（
			return {};
		}
		std::string_view body = s.substr(0, s.size() - 1);
		std::string_view tone = s.substr(s.size() - 1);

		if (MapLocal != std::nullopt) {//不需要了，映射逻辑交给音素类reload方法完成
			const std::map<std::string_view, std::string_view>& Local = MapLocal.value();
			auto it = Local.find(body);//之前分割的cut其实就和body一致
			if (it != Local.end()) {
				body = it->second;//这个映射是没声调的，确实应该直接赋值
			}
		}
		std::vector<std::string_view> result = cutter(body);
		result.push_back(tone);//取最后一个字符构造字符串(声调)
		return result;
	}

	std::vector<std::string_view> Keyboard::standard(const std::string_view& s) {
		std::vector<std::string_view> result;
		size_t cursor = 0;
		if (hasInitial(s)) {
			cursor = s.size() >= 2 && s[1] == 'h' ? 2 : 1;//原始代码会把2字符的给判断错误，这里写大于等于才是正确的
			result.push_back(s.substr(0, cursor));
		}
		//final
		if (s.size() != cursor) {
			result.push_back(s.substr(cursor, s.size() - cursor));
		}
		return result;
	}

	std::vector<std::string_view> Keyboard::zero(const std::string_view& s) {
		std::vector<std::string_view> ss = standard(s);
		if (ss.size() == 1) {//因为职责改变，所以是1，没有声调
			std::string_view finale = ss[0];//取字符串第一个元素
			ss[0] = finale.substr(0, 1);//覆写第一个元素为其字符串开头的字符
			if (finale.size() == 2) {
				ss.emplace_back(finale.substr(1, 1));//第二个字符，长度1
			}
			else {
				ss.emplace_back(finale);//因为职责改变，去除了声调在这里，所以只有一个音素的情况下，直接最后追加即可
			}
		}
		return ss;
	}

	//文件内私有
	const static std::map<std::string, std::string> DAQIAN_KEYS = std::map<std::string, std::string>({
		{"", ""}, {"0", ""}, {"1", " "}, {"2", "6"}, {"3", "3"},
		{"4", "4"}, {"a", "8"}, {"ai", "9"}, {"an", "0"}, {"ang", ";"},
		{"ao", "l"}, {"b", "1"}, {"c", "h"}, {"ch", "t"}, {"d", "2"},
		{"e", "k"}, {"ei", "o"}, {"en", "p"}, {"eng", "/"}, {"er", "-"},
		{"f", "z"}, {"g", "e"}, {"h", "c"}, {"i", "u"}, {"ia", "u8"},
		{"ian", "u0"}, {"iang", "u;"}, {"iao", "ul"}, {"ie", "u,"}, {"in", "up"},
		{"ing", "u/"}, {"iong", "m/"}, {"iu", "u."}, {"j", "r"}, {"k", "d"},
		{"l", "x"}, {"m", "a"}, {"n", "s"}, {"o", "i"}, {"ong", "j/"},
		{"ou", "."}, {"p", "q"}, {"q", "f"}, {"r", "b"}, {"s", "n"},
		{"sh", "g"}, {"t", "w"}, {"u", "j"}, {"ua", "j8"}, {"uai", "j9"},
		{"uan", "j0"}, {"uang", "j;"}, {"uen", "mp"}, {"ueng", "j/"}, {"ui", "jo"},
		{"un", "jp"}, {"uo", "ji"}, {"v", "m"}, {"van", "m0"}, {"vang", "m;"},
		{"ve", "m,"}, {"vn", "mp"}, {"w", "j"}, {"x", "v"}, {"y", "u"},
		{"z", "y"}, {"zh", "5"},
	});

	const static std::map<std::string, std::string> XIAOHE_KEYS = std::map<std::string, std::string>({
		{"ai", "d"}, {"an", "j"}, {"ang", "h"}, {"ao", "c"}, {"ch", "i"},
		{"ei", "w"}, {"en", "f"}, {"eng", "g"}, {"ia", "x"}, {"ian", "m"},
		{"iang", "l"}, {"iao", "n"}, {"ie", "p"}, {"in", "b"}, {"ing", "k"},
		{"iong", "s"}, {"iu", "q"}, {"ong", "s"}, {"ou", "z"}, {"sh", "u"},
		{"ua", "x"}, {"uai", "k"}, {"uan", "r"}, {"uang", "l"}, {"ui", "v"},
		{"un", "y"}, {"uo", "o"}, {"ve", "t"}, {"ue", "t"}, {"vn", "y"},
		{"zh", "v"},
	});

	const static std::map<std::string, std::string> ZIRANMA_KEYS = std::map<std::string, std::string>({
		{"ai", "l"}, {"an", "j"}, {"ang", "h"}, {"ao", "k"}, {"ch", "i"},
		{"ei", "z"}, {"en", "f"}, {"eng", "g"}, {"ia", "w"}, {"ian", "m"},
		{"iang", "d"}, {"iao", "c"}, {"ie", "x"}, {"in", "n"}, {"ing", "y"},
		{"iong", "s"}, {"iu", "q"}, {"ong", "s"}, {"ou", "b"}, {"sh", "u"},
		{"ua", "w"}, {"uai", "y"}, {"uan", "r"}, {"uang", "d"}, {"ui", "v"},
		{"un", "p"}, {"uo", "o"}, {"ve", "t"}, {"ue", "t"}, {"vn", "p"},
		{"zh", "v"},
	});

	const static std::map<std::string, std::string> PHONETIC_LOCAL = std::map<std::string, std::string>({
		{"yi", "i"}, {"you", "iu"}, {"yin", "in"}, {"ye", "ie"}, {"ying", "ing"},
		{"wu", "u"}, {"wen", "un"}, {"yu", "v"}, {"yue", "ve"}, {"yuan", "van"},
		{"yun", "vn"}, {"ju", "jv"}, {"jue", "jve"}, {"juan", "jvan"}, {"jun", "jvn"},
		{"qu", "qv"}, {"que", "qve"}, {"quan", "qvan"}, {"qun", "qvn"}, {"xu", "xv"},
		{"xue", "xve"}, {"xuan", "xvan"}, {"xun", "xvn"}, {"shi", "sh"}, {"si", "s"},
		{"chi", "ch"}, {"ci", "c"}, {"zhi", "zh"}, {"zi", "z"}, {"ri", "r"},
	});

	const static std::map<std::string, std::string> SOUGOU_KEYS = std::map<std::string, std::string>({
		{"ai", "l"}, {"an", "j"}, {"ang", "h"}, {"ao", "k"}, {"ch", "i"},
		{"ei", "z"}, {"en", "f"}, {"eng", "g"}, {"ia", "w"}, {"ian", "m"},
		{"iang", "d"}, {"iao", "c"}, {"ie", "x"}, {"in", "n"}, {"ing", ";"},
		{"iong", "s"}, {"iu", "q"}, {"ong", "s"}, {"ou", "b"}, {"sh", "u"},
		{"ua", "w"}, {"uai", "y"}, {"uan", "r"}, {"uang", "d"}, {"ui", "v"},
		{"un", "p"}, {"uo", "o"}, {"ve", "t"}, {"ue", "t"}, {"v", "y"},
		{"zh", "v"}
	});

	const static std::map<std::string, std::string> ZHINENG_ABC_KEYS = std::map<std::string, std::string>({
		{"ai", "l"}, {"an", "j"}, {"ang", "h"}, {"ao", "k"}, {"ch", "e"},
		{"ei", "q"}, {"en", "f"}, {"eng", "g"}, {"er", "r"}, {"ia", "d"},
		{"ian", "w"}, {"iang", "t"}, {"iao", "z"}, {"ie", "x"}, {"in", "c"},
		{"ing", "y"}, {"iong", "s"}, {"iu", "r"}, {"ong", "s"}, {"ou", "b"},
		{"sh", "v"}, {"ua", "d"}, {"uai", "c"}, {"uan", "p"}, {"uang", "t"},
		{"ui", "m"}, {"un", "n"}, {"uo", "o"}, {"ve", "v"}, {"ue", "m"},
		{"zh", "a"},
	});

	const static std::map<std::string, std::string> GUOBIAO_KEYS = std::map<std::string, std::string>({
		{"ai", "k"}, {"an", "f"}, {"ang", "g"}, {"ao", "c"}, {"ch", "i"},
		{"ei", "b"}, {"en", "r"}, {"eng", "h"}, {"er", "l"}, {"ia", "q"},
		{"ian", "d"}, {"iang", "n"}, {"iao", "m"}, {"ie", "t"}, {"in", "l"},
		{"ing", "j"}, {"iong", "s"}, {"iu", "y"}, {"ong", "s"}, {"ou", "p"},
		{"sh", "u"}, {"ua", "q"}, {"uai", "y"}, {"uan", "w"}, {"uang", "n"},
		{"ui", "v"}, {"un", "z"}, {"uo", "o"}, {"van", "w"}, {"ve", "x"},
		{"vn", "z"}, {"zh", "v"},
	});

	const static std::map<std::string, std::string> MICROSOFT_KEYS = std::map<std::string, std::string>({
		{"ai", "l"}, {"an", "j"}, {"ang", "h"}, {"ao", "k"}, {"ch", "i"},
		{"ei", "z"}, {"en", "f"}, {"eng", "g"}, {"er", "r"}, {"ia", "w"},
		{"ian", "m"}, {"iang", "d"}, {"iao", "c"}, {"ie", "x"}, {"in", "n"},
		{"ing", ";"}, {"iong", "s"}, {"iu", "q"}, {"ong", "s"}, {"ou", "b"},
		{"sh", "u"}, {"ua", "w"}, {"uai", "y"}, {"uan", "r"}, {"uang", "d"},
		{"ui", "v"}, {"un", "p"}, {"uo", "o"}, {"ve", "v"}, {"ue", "t"},
		{"v", "y"}, {"zh", "v"}
	});

	const static std::map<std::string, std::string> PINYINPP_KEYS = std::map<std::string, std::string>({
		{"ai", "s"}, {"an", "f"}, {"ang", "g"}, {"ao", "d"}, {"ch", "u"},
		{"ei", "w"}, {"en", "r"}, {"eng", "t"}, {"er", "q"}, {"ia", "b"},
		{"ian", "j"}, {"iang", "h"}, {"iao", "k"}, {"ie", "m"}, {"in", "l"},
		{"ing", "q"}, {"iong", "y"}, {"iu", "n"}, {"ong", "y"}, {"ou", "p"},
		{"ua", "b"}, {"uai", "x"}, {"uan", "c"}, {"uang", "h"}, {"ue", "x"},
		{"ui", "v"}, {"un", "z"}, {"uo", "o"}, {"sh", "i"}, {"zh", "v"}
	});

	const static std::map<std::string, std::string> ZIGUANG_KEYS = std::map<std::string, std::string>({
		{"ai", "p"}, {"an", "r"}, {"ang", "s"}, {"ao", "q"}, {"ch", "a"},
		{"ei", "k"}, {"en", "w"}, {"eng", "t"}, {"er", "j"}, {"ia", "x"},
		{"ian", "f"}, {"iang", "g"}, {"iao", "b"}, {"ie", "d"}, {"in", "y"},
		{"ing", ";"}, {"iong", "h"}, {"iu", "j"}, {"ong", "h"}, {"ou", "z"},
		{"ua", "x"}, {"uan", "l"}, {"uai", "y"}, {"uang", "g"}, {"ue", "n"},
		{"un", "m"}, {"uo", "o"}, {"ve", "n"}, {"sh", "i"}, {"zh", "u"},
	});

	const Keyboard Keyboard::QUANPIN = Keyboard(std::nullopt, std::nullopt, Keyboard::standard, false, true);
	const Keyboard Keyboard::DAQIAN = Keyboard(PHONETIC_LOCAL, DAQIAN_KEYS, Keyboard::standard, false, false);
	const Keyboard Keyboard::XIAOHE = Keyboard(std::nullopt, XIAOHE_KEYS, Keyboard::zero, true, false);
	const Keyboard Keyboard::ZIRANMA = Keyboard(std::nullopt, ZIRANMA_KEYS, Keyboard::zero, true, false);
	const Keyboard Keyboard::SOUGOU = Keyboard(std::nullopt, SOUGOU_KEYS, Keyboard::zero, true, false);
	const Keyboard Keyboard::GUOBIAO = Keyboard(std::nullopt, GUOBIAO_KEYS, Keyboard::zero, true, false);
	const Keyboard Keyboard::MICROSOFT = Keyboard(std::nullopt, MICROSOFT_KEYS, Keyboard::zero, true, false);
	const Keyboard Keyboard::PINYINPP = Keyboard(std::nullopt, PINYINPP_KEYS, Keyboard::zero, true, false);
	const Keyboard Keyboard::ZIGUANG = Keyboard(std::nullopt, ZIGUANG_KEYS, Keyboard::zero, true, false);
}
