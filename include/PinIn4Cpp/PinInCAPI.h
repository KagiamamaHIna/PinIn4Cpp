#pragma once

#include "PinIn4Cpp/detail/auto/pinin4cpp_export.h"

//CAPI的版本主要是提供一套基础的接口，能完成基本的搜索任务为前提的，更复杂的为何不考虑用C++构建呢？
//如果你想快速为其他语言提供一套单线程的搜索API（其实也是这个库的核心任务）
//那这个CAPI可能会以一个起点来讲很合适
//所以暂时不考虑写ParallelSearch到CAPI中，也就是CAPI中的只能单线程，CAPI的版本也不支持 从内存反序列化/序列化 是直接交互文件api的，主要是避免麻烦

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

	struct PinInCpp_PinIn_NULL { int unused; };
	typedef struct PinInCpp_PinIn_NULL* PinInCpp_PinIn;//PinInCpp_PinIn == std::shared_ptr<PinInCpp::PinIn> *
	//需要在C++层面共享所有权，所以是套的智能指针，但是一定要记得回收这个智能指针，不然会内存泄漏

	struct PinInCpp_TreeSearcher_NULL { int unused; };
	typedef struct PinInCpp_TreeSearcher_NULL* PinInCpp_TreeSearcher;//PinInCpp_TreeSearcher == PinInCpp::TreeSearcher * 
	//因为不需要在C++层面共享所有权的，所以没封装

	enum PinInCpp_TreeSeracher_Keyboard {//不提供C情况下的自定义方案，因为那可能过于麻烦，预设的也足够好了
		PinInCpp_NULLKeyboard,	//代表不改变Keyboard，特殊的枚举量
		PinInCpp_QUANPIN,		//基础的全拼方案 PinIn类的默认方案
		PinInCpp_DAQIAN,		//注音（大千）输入法方案
		PinInCpp_XIAOHE,		//小鹤双拼方案
		PinInCpp_ZIRANMA,		//自然码双拼方案
		PinInCpp_SOUGOU,		//搜狗双拼方案
		PinInCpp_ZHINENG_ABC,	//智能ABC双拼方案
		PinInCpp_GUOBIAO,		//国标双拼方案
		PinInCpp_MICROSOFT,		//微软双拼方案
		PinInCpp_PINYINPP,		//拼音加加双拼方案
		PinInCpp_ZIGUANG,		//紫光双拼方案
	};
	typedef enum PinInCpp_TreeSeracher_Keyboard PinInCpp_TreeSeracher_Keyboard;

	enum PinInCpp_TreeSeracher_Logic {
		PinInCpp_BEGIN,
		PinInCpp_CONTAIN,
		PinInCpp_EQUAL,
	};
	typedef enum PinInCpp_TreeSeracher_Logic PinInCpp_TreeSeracher_Logic;

	enum PinInCpp_DeserializeError {
		PinInCpp_DeserNormal,
		PinInCpp_FileNotOpen,
		PinInCpp_DeserBinaryVersionInvalidException,
		PinInCpp_DeserOutOfRange,
		PinInCpp_DeserBadAlloc
	};
	typedef enum PinInCpp_DeserializeError PinInCpp_DeserializeError;

	struct PinInCpp_Config {
		PinInCpp_TreeSeracher_Keyboard keyboard;//返回的时候默认 PinInCpp_NULLKeyboard 因为我也没办法确定Keyboard是谁
		char fZh2Z;//把他们当成bool用(
		char fSh2S;
		char fCh2C;
		char fAng2An;
		char fIng2In;
		char fEng2En;
		char fU2V;
		char fFirstChar;
	};
	typedef struct PinInCpp_Config PinInCpp_Config;

	//用PinInCpp_SearchResult_Free回收内存
	struct PinInCpp_SearchResult {
		size_t* ids;
		size_t size;
	};
	typedef struct PinInCpp_SearchResult PinInCpp_SearchResult;
	PININ4CPP_EXPORT void PinInCpp_SearchResult_Free(PinInCpp_SearchResult result);

	//依赖字典文件路径初始化。获取资源时需要检查空指针
	PININ4CPP_EXPORT PinInCpp_PinIn PinInCpp_PinIn_New(const char* path);
	PININ4CPP_EXPORT void PinInCpp_PinIn_Free(PinInCpp_PinIn pinin);

	//依赖序列化文件初始化，第三个参数承担返回值的工作
	PININ4CPP_EXPORT PinInCpp_DeserializeError PinInCpp_PinIn_Deserialize(const char* path, PinInCpp_TreeSeracher_Keyboard keyboard, PinInCpp_PinIn* pinin);
	PININ4CPP_EXPORT int PinInCpp_PinIn_Serialize(PinInCpp_PinIn pinin, const char* path);//返回值为真则写入成功

	//检查有效性
	PININ4CPP_EXPORT int PinInCpp_PinIn_Empty(PinInCpp_PinIn pinin);

	//配置接口
	PININ4CPP_EXPORT PinInCpp_Config PinInCpp_PinIn_GetConfig(PinInCpp_PinIn pinin);
	PININ4CPP_EXPORT void PinInCpp_PinIn_ConfigCommit(PinInCpp_PinIn pinin, PinInCpp_Config cfg);

	//PinIn缓存控制接口
	PININ4CPP_EXPORT void PinInCpp_PinIn_PreCacheString(PinInCpp_PinIn pinin, const char* str);
	PININ4CPP_EXPORT void PinInCpp_PinIn_PreNullPinyinIdCache(PinInCpp_PinIn pinin);
	PININ4CPP_EXPORT int PinInCpp_PinIn_IsCharCacheEnabled(PinInCpp_PinIn pinin);
	PININ4CPP_EXPORT void PinInCpp_PinIn_SetCharCache(PinInCpp_PinIn pinin, int enable);

	//获取资源时需要检查空指针
	PININ4CPP_EXPORT PinInCpp_TreeSearcher PinInCpp_TreeSearcher_NewPath(PinInCpp_TreeSeracher_Logic logic, const char* path);
	PININ4CPP_EXPORT PinInCpp_TreeSearcher PinInCpp_TreeSearcher_NewPinIn(PinInCpp_TreeSeracher_Logic logic, PinInCpp_PinIn pinin);
	PININ4CPP_EXPORT void PinInCpp_TreeSearcher_Free(PinInCpp_TreeSearcher pinin);

	//依赖序列化文件初始化，第三个参数承担返回值的工作
	PININ4CPP_EXPORT PinInCpp_DeserializeError PinInCpp_TreeSearcher_Deserialize(const char* path, PinInCpp_PinIn pinin, PinInCpp_TreeSearcher* tree);
	PININ4CPP_EXPORT int PinInCpp_TreeSearcher_Serialize(PinInCpp_TreeSearcher tree, const char* path);//返回值为真则写入成功

	//需要检查空指针，从搜索树上获取PinIn
	PININ4CPP_EXPORT PinInCpp_PinIn PinInCpp_TreeSearcher_GetPinIn(PinInCpp_TreeSearcher tree);

	//搜索树的搜索API
	PININ4CPP_EXPORT size_t PinInCpp_TreeSearcher_PutString(PinInCpp_TreeSearcher tree, const char* str);//插入待搜索项
	PININ4CPP_EXPORT PinInCpp_SearchResult PinInCpp_TreeSearcher_ExecuteSearch(PinInCpp_TreeSearcher tree, const char* str);//获取结果列表，需要手动调用PinInCpp_SearchResultFree
	PININ4CPP_EXPORT size_t PinInCpp_TreeSearcher_GetStrSizeById(PinInCpp_TreeSearcher tree, size_t id);//使用PinInCpp_SearchResult里的id
	PININ4CPP_EXPORT int PinInCpp_TreeSearcher_PutToCharBuf(PinInCpp_TreeSearcher tree, size_t id, char* buf, size_t bufSize);//根据提供的缓冲区填充字符串，如果数据因为缓冲区大小被截断了，那么返回的是-1。完整的插入了则是0

	//搜索树的其他控制接口
	PININ4CPP_EXPORT void PinInCpp_TreeSearcher_StrPoolReserve(PinInCpp_TreeSearcher tree, size_t _Newcapacity);
	PININ4CPP_EXPORT void PinInCpp_TreeSearcher_Refresh(PinInCpp_TreeSearcher tree);//手动尝试刷新
	//It is no longer of use / 不再有用
	PININ4CPP_EXPORT void PinInCpp_TreeSearcher_ClearFreeList(PinInCpp_TreeSearcher);
	PININ4CPP_EXPORT void PinInCpp_TreeSearcher_ShrinkToFit(PinInCpp_TreeSearcher tree);
#ifdef __cplusplus
}
#endif
