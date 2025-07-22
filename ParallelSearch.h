#pragma once
#include <thread>
#include <barrier>
#include <mutex>

#include "TreeSearcher.h"

namespace PinInCpp {
	class ParallelSearch {//并行搜索树逻辑，如果数据量比较小（如4w行），则无明显效果，按需使用
	public:
		ParallelSearch(Logic logic, const std::string& PinyinDictionaryPath, size_t TreeNum)
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
		ParallelSearch(const ParallelSearch&) = delete;
		ParallelSearch(ParallelSearch&&) = delete;
		ParallelSearch& operator=(ParallelSearch&& src) = delete;

		std::vector<std::string> ExecuteSearch(const std::string_view& str) {//只需要一个线程执行这个函数即可并发搜索，不要用多个线程执行此函数
			CommonSearch(str);
			std::vector<std::string> result;
			for (const auto& vec : ResultSet) {
				for (const auto& str : vec) {
					result.push_back(std::string(str));
				}
			}
			return result;
		}
		std::vector<std::string_view> ExecuteSearchView(const std::string_view& str) {//只需要一个线程执行这个函数即可并发搜索，不要用多个线程执行此函数。返回的只读视图会在插入后可能变成悬垂视图
			CommonSearch(str);
			std::vector<std::string_view> result;
			for (const auto& vec : ResultSet) {
				for (const auto& str : vec) {
					result.push_back(str);
				}
			}
			return result;
		}
		//线程不安全，你应该在单线程内执行它
		void put(const std::string_view& keyword) {
			context->PreCacheString(keyword);//手动预热

			ClearResultSet = true;//一个flag，通知搜索的时候清空结果集，因为put可能会导致视图失效
			TreePool[NextIndex]->put(keyword);
			NextIndex++;
			if (NextIndex >= TreeNum) {
				NextIndex = 0;
			}
		}
		//单位是字节
		void StrPoolReserve(size_t index, size_t _Newcapacity) {
			TreePool.at(index)->StrPoolReserve(_Newcapacity);
		}

		PinIn& GetPinIn() {
			return *context;
		}
		const PinIn& GetPinIn()const {
			return *context;
		}
		std::shared_ptr<PinIn> GetPinInShared() {//返回这个对象的智能指针，让你可以共享到其他TreeSearcher
			return context;
		}
	private:
		void init(Logic logic) {
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
		void CommonSearch(const std::string_view& str) {//只需要一个线程执行这个函数即可并发搜索，不要用多个线程执行此函数
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
