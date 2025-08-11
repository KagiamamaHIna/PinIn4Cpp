#pragma once
#include <thread>
#include <barrier>
#include <mutex>

#include "TreeSearcher.h"

namespace PinInCpp {
	class ParallelSearchTreeNumException : public std::exception {
	public:
		virtual const char* what() {
			return "tree number cannot be 0";
		}
	};
	class ParallelSearch {//并行搜索树逻辑，如果数据量比较小（如4w行），则无明显效果，按需使用
	public:
		ParallelSearch(Logic logic, std::string_view PinyinDictionaryPath, size_t TreeNum)
			:context(std::make_shared<PinIn>(PinyinDictionaryPath)), TreeNum{ TreeNum }, barrier(TreeNum + 1) {
			init(logic);
		}
		ParallelSearch(Logic logic, const std::vector<char>& PinyinDictionaryData, size_t TreeNum)
			:context(std::make_shared<PinIn>(PinyinDictionaryData)), TreeNum{ TreeNum }, barrier(TreeNum + 1) {
			init(logic);
		}
		ParallelSearch(Logic logic, std::shared_ptr<PinIn> PinInShared, size_t TreeNum)
			:context(PinInShared), TreeNum{ TreeNum }, barrier(TreeNum + 1) {//工作线程数 + 1 (主线程),主线程被堵塞等待搜索完成
			init(logic);
		}
		~ParallelSearch() {
			if (!StopFlag) {
				StopFlag = true;
				barrier.arrive_and_wait();
			}
			for (auto& v : ThreadPool) {
				if (v.joinable()) {
					v.join();
				}
			}
		}

		//32位环境和64位环境生成的文件格式应该是通用的，但是如果64位下数据量过大，32位环境下加载有潜在的溢出导致逻辑错误的风险
		//你应该只传入对应类的Serialization方法生成的数据，传入一个不正确格式的很有可能会造成未定义行为
		//抛出BinaryVersionInvalidException代表版本号错误，不可使用
		//请自行构建一个合适的PinIn类实例，PinIn类本身也可以序列化+反序列化
		static std::unique_ptr<ParallelSearch> Deserialize(const std::vector<uint8_t>& data, std::shared_ptr<PinIn> PinInShared, size_t index = 0) {
			uint32_t ver = GetU8VecDW(data, index);
			if (ver != BinDataVersion) {
				throw BinaryVersionInvalidException("ParallelSearch: Invalid binary file version");
			}
			index += 4;

			Logic logic = static_cast<Logic>(data[index]);
			index++;

			size_t NextIndex = static_cast<size_t>(GetU8VecQW(data, index));
			index += 8;

			size_t TreeNum = static_cast<size_t>(GetU8VecQW(data, index));
			index += 8;

			std::unique_ptr<ParallelSearch> result = std::make_unique<ParallelSearch>(logic, PinInShared, TreeNum);
			result->NextIndex = NextIndex;

			for (size_t i = 0; i < TreeNum; i++) {
				size_t TreeSize = static_cast<size_t>(GetU8VecQW(data, index));
				index += 8;
				result->TreePool[i] = TreeSearcher::Deserialize(data, PinInShared, index);
				index += TreeSize;
			}
			PinInShared->PreCacheDeserialize(data, index);//加载缓存数据
			return result;
		}
		static std::optional<std::unique_ptr<ParallelSearch>> DeserializeFromFile(std::string_view path, std::shared_ptr<PinIn> PinInShared, size_t index = 0) {
			std::optional<std::vector<uint8_t>> data = ReadBinFile(std::string(path));
			if (!data.has_value()) {
				return std::nullopt;
			}
			return Deserialize(data.value(), PinInShared, index);
		}
		std::vector<uint8_t> Serialize()const {
			std::vector<uint8_t> result;
			PushDWUint8(result, BinDataVersion);
			result.push_back(static_cast<uint8_t>(TreePool[0]->GetLogic()));
			PushQWUint8(result, NextIndex);
			PushQWUint8(result, TreeNum);

			for (const auto& v : TreePool) {
				std::vector<uint8_t> temp = v->Serialize();
				PushQWUint8(result, temp.size());
				result.insert(result.end(), temp.begin(), temp.end());
			}

			std::vector<uint8_t> temp = context->PreCacheSerialize();
			result.insert(result.end(), temp.begin(), temp.end());//记录缓存数据，防止反序列化回来的时候因为缺少缓存导致线程不安全

			return result;
		}
		//返回真代表写入成功
		bool SerializationToFile(std::string_view path)const {
			return WriteBinFile(std::string(path), Serialize());
		}

		ParallelSearch(const ParallelSearch&) = delete;
		ParallelSearch(ParallelSearch&&) = delete;
		ParallelSearch& operator=(ParallelSearch&& src) = delete;

		std::vector<std::string> ExecuteSearch(std::string_view str) {//只需要一个线程执行这个函数即可并发搜索，不要用多个线程执行此函数
			CommonSearch(str);
			std::vector<std::string> result;
			for (const auto& vec : ResultSet) {
				for (const auto& str : vec) {
					result.emplace_back(str);
				}
			}
			return result;
		}
		std::vector<std::string_view> ExecuteSearchView(std::string_view str) {//只需要一个线程执行这个函数即可并发搜索，不要用多个线程执行此函数。返回的只读视图会在插入后可能变成悬垂视图
			CommonSearch(str);
			std::vector<std::string_view> result;
			for (const auto& vec : ResultSet) {
				for (const auto& str : vec) {
					result.emplace_back(str);
				}
			}
			return result;
		}
		//线程不安全，你应该在单线程内执行它
		void put(std::string_view keyword) {
			context->PreCacheString(keyword);//手动预热

			ClearResultSet = true;//一个flag，通知搜索的时候清空结果集，因为put可能会导致视图失效
			TreePool[NextIndex]->put(keyword);
			NextIndex++;
			if (NextIndex >= TreeNum) {
				NextIndex = 0;
			}
		}
		size_t GetTreeNum()const noexcept {
			return TreeNum;
		}

		//单位是字节
		void StrPoolReserve(size_t index, size_t _Newcapacity) {
			TreePool.at(index)->StrPoolReserve(_Newcapacity);
		}

		void ClearFreeList() {
			for (const auto& v : TreePool) {
				v->ClearFreeList();
			}
		}

		void ShrinkToFit() {
			for (const auto& v : TreePool) {
				v->ShrinkToFit();
			}
		}

		PinIn& GetPinIn() noexcept {
			return *context;
		}
		const PinIn& GetPinIn()const noexcept {
			return *context;
		}
		std::shared_ptr<PinIn> GetPinInShared()noexcept {//返回这个对象的智能指针，让你可以共享到其他TreeSearcher
			return context;
		}
	private:
		void init(Logic logic) {
			if (TreeNum == 0) {
				throw ParallelSearchTreeNumException();
			}
			context->PreNullPinyinIdCache();
			ticket = context->ticket([this]() {
				ClearResultSet = true;
			});

			ThreadPool.reserve(TreeNum);
			TreePool.reserve(TreeNum);
			for (size_t i = 0; i < TreeNum; i++) {
				TreePool.push_back(std::make_unique<TreeSearcher>(logic, context));
				ThreadPool.emplace_back([this, i]() {
					while (true) {
						// 1. 等待开始信号，同时也是上一轮的结束点
						barrier.arrive_and_wait();
						if (StopFlag) {//收到结束信号就退出循环
							break;
						}
						// 2. 执行任务，并放入结果集数组
						ResultSet[i] = TreePool[i]->ExecuteSearchView(searchStr);
						// 3. 任务完成，到达屏障等待其他线程
						barrier.arrive_and_wait();
					}
				});
			}
		}
		void CommonSearch(std::string_view str) {//只需要一个线程执行这个函数即可并发搜索，不要用多个线程执行此函数
			ticket->renew();
			if (str != searchStr || ClearResultSet) {//如果是新搜索项或者需要清空结果集时，唤醒线程执行多线程搜索逻辑
				ClearResultSet = false;
				ResultSet.resize(TreeNum);//清空并留下空余数组，以方便多线程的时候插入数据
				context->PreCacheString(str);//预热
				searchStr = str;
				//发出信号唤醒线程
				barrier.arrive_and_wait();
				//等待线程执行完成
				barrier.arrive_and_wait();
			}
		}
		std::shared_ptr<PinIn> context;//共享状态
		std::vector<std::thread> ThreadPool;//线程池
		std::vector<std::unique_ptr<TreeSearcher>> TreePool;//树池
		std::vector<std::vector<std::string_view>> ResultSet;//用一个数组管理应该插入的数据
		std::unique_ptr<PinIn::Ticket> ticket;
		std::string searchStr;
		const size_t TreeNum;
		size_t NextIndex = 0;
		bool ClearResultSet = false;

		std::atomic<bool> StopFlag = false;
		std::barrier<> barrier;
	};
}
