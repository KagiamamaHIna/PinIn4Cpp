#include "PinIn4Cpp/PinInCAPI.h"
#include "PinIn4Cpp/TreeSearcher.h"
#include <stdlib.h>

static std::optional<PinInCpp::Keyboard> GetKeyboardFromEnum(PinInCpp_TreeSeracher_Keyboard keyboard) {
	std::optional<PinInCpp::Keyboard> key;
	switch (keyboard) {
	case PinInCpp_QUANPIN: {
		key = PinInCpp::Keyboard::QUANPIN;
		break;
	}
	case PinInCpp_DAQIAN: {
		key = PinInCpp::Keyboard::DAQIAN;
		break;
	}
	case PinInCpp_XIAOHE: {
		key = PinInCpp::Keyboard::XIAOHE;
		break;
	}
	case PinInCpp_ZIRANMA: {
		key = PinInCpp::Keyboard::ZIRANMA;
		break;
	}
	case PinInCpp_SOUGOU: {
		key = PinInCpp::Keyboard::SOUGOU;
		break;
	}
	case PinInCpp_ZHINENG_ABC: {
		key = PinInCpp::Keyboard::ZHINENG_ABC;
		break;
	}
	case PinInCpp_GUOBIAO: {
		key = PinInCpp::Keyboard::GUOBIAO;
		break;
	}
	case PinInCpp_MICROSOFT: {
		key = PinInCpp::Keyboard::MICROSOFT;
		break;
	}
	case PinInCpp_PINYINPP: {
		key = PinInCpp::Keyboard::PINYINPP;
		break;
	}
	case PinInCpp_ZIGUANG: {
		key = PinInCpp::Keyboard::ZIGUANG;
		break;
	}
	case PinInCpp_NULLKeyboard: {
		break;
	}
	}
	return key;
}

void PinInCpp_SearchResult_Free(PinInCpp_SearchResult result) {
	free(result.ids);
}

PinInCpp_PinIn PinInCpp_PinIn_New(const char* path) {
	PinInCpp::PinIn* rawPtr;
	try {
		rawPtr = new PinInCpp::PinIn(path);
	}
	catch (std::bad_alloc&) {
		return NULL;//使用c宏代表空指针
	}
	catch (PinInCpp::PinyinFileNotOpenException&) {
		return NULL;
	}

	std::shared_ptr<PinInCpp::PinIn>* result;
	try {
		result = new std::shared_ptr<PinInCpp::PinIn>();
	}
	catch (std::bad_alloc&) {
		delete rawPtr;//如果抛出bad_alloc异常了，那么析构掉数据
		return NULL;
	}
	result->reset(rawPtr);
	return (PinInCpp_PinIn)result;
}

PinInCpp_DeserializeError PinInCpp_PinIn_Deserialize(const char* path, PinInCpp_TreeSeracher_Keyboard keyboard, PinInCpp_PinIn* pinin) {
	std::optional<std::shared_ptr<PinInCpp::PinIn>> result;
	try {
		result = PinInCpp::PinIn::DeserializeFromFile(path, GetKeyboardFromEnum(keyboard));
	}
	catch (std::out_of_range&) {
		return PinInCpp_DeserOutOfRange;
	}
	catch (PinInCpp::BinaryVersionInvalidException&) {
		return PinInCpp_DeserBinaryVersionInvalidException;
	}
	catch (std::bad_alloc&) {
		return PinInCpp_DeserBadAlloc;
	}

	if (!result.has_value()) {//不含有值
		return PinInCpp_FileNotOpen;
	}

	try {
		//覆写指针
		*pinin = (PinInCpp_PinIn)new std::shared_ptr<PinInCpp::PinIn>(std::move(result.value()));//直接移动智能指针即可
	}
	catch (std::bad_alloc&) {
		return PinInCpp_DeserBadAlloc;
	}

	return PinInCpp_DeserNormal;
}

int PinInCpp_PinIn_Serialize(PinInCpp_PinIn pinin, const char* path) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	return p->get()->SerializeToFile(path);
}

void PinInCpp_PinIn_Free(PinInCpp_PinIn pinin) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	delete p;
}

int PinInCpp_PinIn_Empty(PinInCpp_PinIn pinin) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	return p->get()->empty();
}

PinInCpp_Config PinInCpp_PinIn_GetConfig(PinInCpp_PinIn pinin) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	PinInCpp_Config result;

	result.keyboard = PinInCpp_NULLKeyboard;
	result.fZh2Z = p->get()->getfZh2Z();
	result.fSh2S = p->get()->getfSh2S();
	result.fCh2C = p->get()->getfCh2C();
	result.fAng2An = p->get()->getfAng2An();
	result.fIng2In = p->get()->getfIng2In();
	result.fEng2En = p->get()->getfEng2En();
	result.fU2V = p->get()->getfU2V();
	result.fFirstChar = p->get()->getfFirstChar();

	return result;
}

void PinInCpp_PinIn_ConfigCommit(PinInCpp_PinIn pinin, PinInCpp_Config cfg) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	PinInCpp::PinIn::Config result = p->get()->config();
	std::optional<PinInCpp::Keyboard> keyboard = GetKeyboardFromEnum(cfg.keyboard);
	if (keyboard.has_value()) {
		result.keyboard = std::move(keyboard.value());
	}
	result.fZh2Z = cfg.fZh2Z;
	result.fSh2S = cfg.fSh2S;
	result.fCh2C = cfg.fCh2C;
	result.fAng2An = cfg.fAng2An;
	result.fIng2In = cfg.fIng2In;
	result.fEng2En = cfg.fEng2En;
	result.fU2V = cfg.fU2V;
	result.fFirstChar = cfg.fFirstChar;

	result.commit();
}

void PinInCpp_PinIn_PreCacheString(PinInCpp_PinIn pinin, const char* str) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	p->get()->PreCacheString(str);
}

void PinInCpp_PinIn_PreNullPinyinIdCache(PinInCpp_PinIn pinin) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	p->get()->PreNullPinyinIdCache();
}

int PinInCpp_PinIn_IsCharCacheEnabled(PinInCpp_PinIn pinin) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	return p->get()->IsCharCacheEnabled();
}

void PinInCpp_PinIn_SetCharCache(PinInCpp_PinIn pinin, int enable) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	p->get()->SetCharCache(enable);
}

PinInCpp_TreeSearcher PinInCpp_TreeSearcher_NewPath(PinInCpp_TreeSeracher_Logic logic, const char* path) {
	try {
		return (PinInCpp_TreeSearcher)new PinInCpp::TreeSearcher(static_cast<PinInCpp::Logic>(logic), path);
	}
	catch (std::bad_alloc&) {
		return NULL;//使用c宏代表空指针
	}
	catch (PinInCpp::BinaryVersionInvalidException&) {
		return NULL;
	}
	catch (PinInCpp::PinyinFileNotOpenException&) {
		return NULL;
	}
}

PinInCpp_TreeSearcher PinInCpp_TreeSearcher_NewPinIn(PinInCpp_TreeSeracher_Logic logic, PinInCpp_PinIn data) {
	try {
		return (PinInCpp_TreeSearcher)new PinInCpp::TreeSearcher(static_cast<PinInCpp::Logic>(logic), *(std::shared_ptr<PinInCpp::PinIn>*)data);
	}
	catch (std::bad_alloc&) {
		return NULL;//使用c宏代表空指针
	}
}

void PinInCpp_TreeSearcher_Free(PinInCpp_TreeSearcher tree) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	delete t;
}

PinInCpp_DeserializeError PinInCpp_TreeSearcher_Deserialize(const char* path, PinInCpp_PinIn pinin, PinInCpp_TreeSearcher* tree) {
	std::shared_ptr<PinInCpp::PinIn>* p = (std::shared_ptr<PinInCpp::PinIn>*)pinin;
	std::optional<std::unique_ptr<PinInCpp::TreeSearcher>> result;
	try {
		result = PinInCpp::TreeSearcher::DeserializeFromFile(path, *p);
	}
	catch (std::out_of_range&) {
		return PinInCpp_DeserOutOfRange;
	}
	catch (PinInCpp::BinaryVersionInvalidException&) {
		return PinInCpp_DeserBinaryVersionInvalidException;
	}
	catch (std::bad_alloc&) {
		return PinInCpp_DeserBadAlloc;
	}
	if (!result.has_value()) {//不含有值
		return PinInCpp_FileNotOpen;
	}

	*tree = (PinInCpp_TreeSearcher)result.value().release();
	return PinInCpp_DeserNormal;
}

int PinInCpp_TreeSearcher_Serialize(PinInCpp_TreeSearcher tree, const char* path) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	return t->SerializeToFile(path);
}

size_t PinInCpp_TreeSearcher_PutString(PinInCpp_TreeSearcher tree, const char* str) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	return t->put(str);
}

PinInCpp_PinIn PinInCpp_TreeSearcher_GetPinIn(PinInCpp_TreeSearcher tree) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	try {
		return (PinInCpp_PinIn)new std::shared_ptr<PinInCpp::PinIn>(t->GetPinInShared());
	}
	catch (std::bad_alloc&) {
		return NULL;
	}
}

PinInCpp_SearchResult PinInCpp_TreeSearcher_ExecuteSearch(PinInCpp_TreeSearcher tree, const char* str) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	std::unordered_set<size_t> resultSet = t->ExecuteSearchGetSet(str);

	PinInCpp_SearchResult result;
	size_t ResultSize = resultSet.size();

	result.size = ResultSize;
	if (ResultSize == 0) {//没数据那么就不用申请缓冲区
		result.ids = NULL;
		return result;
	}

	result.ids = (size_t*)malloc(sizeof(size_t) * ResultSize);
	if (result.ids == NULL) {
		return result;
	}
	size_t* resultBuf = result.ids;

	for (const size_t item : resultSet) {
		*resultBuf = item;
		resultBuf++;
	}
	
	return result;
}

size_t PinInCpp_TreeSearcher_GetStrSizeById(PinInCpp_TreeSearcher tree, size_t id) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	return t->GetStrSizeById(id);
}

int PinInCpp_TreeSearcher_PutToCharBuf(PinInCpp_TreeSearcher tree, size_t id, char* buf, size_t bufSize) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	return t->PutToCharBufById(id, buf, bufSize);
}

void PinInCpp_TreeSearcher_StrPoolReserve(PinInCpp_TreeSearcher tree, size_t _Newcapacity) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	t->StrPoolReserve(_Newcapacity);
}

void PinInCpp_TreeSearcher_Refresh(PinInCpp_TreeSearcher tree) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	t->refresh();
}

void PinInCpp_TreeSearcher_ClearFreeList(PinInCpp_TreeSearcher) {
	//PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	//t->ClearFreeList();
}

void PinInCpp_TreeSearcher_ShrinkToFit(PinInCpp_TreeSearcher tree) {
	PinInCpp::TreeSearcher* t = (PinInCpp::TreeSearcher*)tree;
	t->ShrinkToFit();
}
