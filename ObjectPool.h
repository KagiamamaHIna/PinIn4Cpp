#pragma once
#include <vector>
#include <stack>
#include <array>
#include <memory>
#include <type_traits>
#include <functional>

namespace PinInCpp {
	//本质上是接管用不到的对象指针，在需要的时候重新构造/构造一个新的对象，如果你自己回收了也没问题，因为分配出去后权限归你
	template<typename T>
	class ObjectPtrPool {
	public:
		static_assert(!std::is_array_v<T>, "Cannot process c array");

		ObjectPtrPool() = default;
		ObjectPtrPool(const ObjectPtrPool&) = delete;
		~ObjectPtrPool() {
			ClearFreeList();
		}
		void ClearFreeList() {
			for (const auto ptr : FreeList) {
				delete ptr;
			}
			FreeList.clear();
			FreeList.shrink_to_fit();//压缩空间
		}
		//你需要将一个指针作为裸指针（比如调用release成员函数）传递进去，由对象池接管这个指针，析构函数会被ObjectPool自动调用，也就是你不用也不要调用析构函数
		//延迟析构的，只会在下一个对象需要分配时才析构这个对象（或者是ObjectPtrPool的ClearFreeList函数被调用了）
		void FreeToPool(T* ptr) {
			FreeList.push_back(ptr);
		}
		template<typename... _Types>
		std::unique_ptr<T> NewObj(_Types&&..._Args) {
			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				return std::make_unique<T>(std::forward<_Types>(_Args)...);
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象，用placement new重新构造后转移所有权
				std::unique_ptr<T> result(FreeList[FreeList.size() - 1]);
				//这里有可能会被vs2022的静态分析报警告 "忽略函数返回值"，但是析构函数没有返回值，所以是误报
				result->~T();//因为ClearFreeList中的delete也会调用析构函数，所以这里延迟到这里调用
				FreeList.pop_back();
				new (result.get()) T(std::forward<_Types>(_Args)...);
				return result;//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
			}
		}
	private:
		std::vector<T*> FreeList;
	};
	//内存池+对象池机制，快速方便的池化对象和内存分配
	//回收对象采用延迟析构模式，只有在对象池本身被析构了/从空闲列表上分配新对象了操作析构应该被析构的对象
	template<typename T, size_t OnePoolSize>
	class ObjectPool {
	public:
		static_assert(!std::is_array_v<T>, "Cannot process c array");
		static_assert(OnePoolSize, "pool size cannot be 0");

		ObjectPool() {
			pool.push(std::array<Block, OnePoolSize>());//压入一块内存
		}
		~ObjectPool() {
			TrueClearMemoryPool();
		}
		ObjectPool(const ObjectPool&) = delete;
		ObjectPool(ObjectPool&&) = delete;
		ObjectPool& operator=(ObjectPool&&) = delete;

		template<typename... _Types>
		std::unique_ptr<T, std::function<void(T*)>> MakeUnique(_Types&&..._Args) {//创建一个独占所有权的智能指针
			return MakeSmartPtrHasDeleter<std::unique_ptr<T, std::function<void(T*)>>>(NewObj(std::forward<_Types>(_Args)...));//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
		}

		template<typename... _Types>
		std::shared_ptr<T> MakeShared(_Types&&..._Args) {//创建一个共享所有权的智能指针
			return MakeSmartPtrHasDeleter<std::shared_ptr<T>>(NewObj(std::forward<_Types>(_Args)...));//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
		}

		//创建一个空的，但绑定好了删除器的unique_ptr
		std::unique_ptr<T, std::function<void(T*)>> MakeUniqueNullHasDeleter() {
			return MakeSmartPtrHasDeleter<std::unique_ptr<T, std::function<void(T*)>>>();
		}

		//创建一个空的，但绑定好了删除器的shared_ptr
		std::shared_ptr<T> MakeSharedNullHasDeleter() {
			return MakeSmartPtrHasDeleter<std::shared_ptr<T>>();
		}

		void FreeListShrinkToFit() {
			FreeList.shrink_to_fit();
		}
		//警告！！！这个api是用于精细管理内存的
		//如果你想抛弃你之前分配的所有对象，让他们的生命周期立刻结束，调用这个能完成你想要的这个操作
		//随后你可以继续用NewObj新建对象，对象池会申请一块新的内存用于分配
		//如果你持有调用这个前由对象池分配的智能指针，调用此函数前记得先释放，当然，完成后也可以，我用共享所有权的智能指针确保了安全性
		void ClearMemoryPool() {
			TrueClearMemoryPool();
			FreeList.clear();//清空空闲列表

			IsDestruction = std::make_shared<bool>(false);//新的指针
			lastRenewUnfinished = NotUnfinished;//重置可能的异常状态
		}
	private:
		constexpr static size_t NotUnfinished = static_cast<size_t>(-1);
		struct Block {
			alignas(T) std::byte b[sizeof(T)];
		};
		void FreeToPool(T* ptr) {
			FreeList.push_back(ptr);
		}

		template<typename retv>
		retv MakeSmartPtrHasDeleter(T* ptr = nullptr) {
			std::weak_ptr<bool> IsDestructionWeak = IsDestruction;//先把所有权共享给函数局部
			auto delFn = [this, IsDestructionWeak](T* ptr) {//传递共享所有权的智能指针进去，确保他能被通知自己管理的内存是否被析构了
				if (!IsDestructionWeak.expired()) {//如果没有被析构
					this->FreeToPool(ptr);
				}
				};
			return retv(ptr, delFn);
		}

		void TrueClearMemoryPool() {
			if (pool.empty()) {
				return;
			}
			IsDestruction = nullptr;//释放内存，std::weak_ptr可以得知观察的指针不存在了，让禁止智能指针调用FreeToPool
			T* lastRenewPtr = nullptr;
			if (lastRenewUnfinished != NotUnfinished) {//如果上一次析构完成但没有成功重分配的话，这个值是真
				lastRenewPtr = FreeList[lastRenewUnfinished];
			}
			while (!pool.empty()) {//因为采用延迟析构实现，所以空闲列表中的指针都不会被真正的析构，只有在分配出去后/这里才会析构
				std::array<Block, OnePoolSize>& arr = pool.top();
				for (size_t i = 0; i < nextpos; i++) {
					T* tmp = reinterpret_cast<T*>(arr.data() + i);
					if (tmp != lastRenewPtr) {//避免重复析构
						tmp->~T();
					}
				}
				nextpos = OnePoolSize;//如果下一次还有，则从下一次的顶部开始指定，结束后也是置顶的，确保潜在的下一次分配生效
				pool.pop();
			}//析构函数手动执行完成后，剩下的内存块就可以安心交给STL
		}

		template<typename... _Types>
		T* NewObj(_Types&&..._Args) {//目标是尝试构造一个对象并返回其裸指针
			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				if (nextpos >= OnePoolSize) {//如果没有空间了，就分配新的一块并重置nextpos
					pool.push(std::array<Block, OnePoolSize>());//压入一块新内存
					nextpos = 0;
				}
				std::array<Block, OnePoolSize>& arr = pool.top();
				T* result = reinterpret_cast<T*>(arr.data() + nextpos);
				new (result) T(std::forward<_Types>(_Args)...);
				nextpos++;//标记位移动延迟到构造完成，如果构造函数抛出异常，则相当于没移动，下一次依旧可以用，如果没有异常则正常移动，避免try块的设计
				return result;
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象，用placement new重新构造后转移所有权
				T* result;
				if (lastRenewUnfinished == NotUnfinished) {
					//这里有可能会被vs2022的静态分析报警告 "忽略函数返回值"，但是析构函数没有返回值，所以是误报
					//延迟到这里析构，主要是方便ObjectPool的析构函数实现
					result = FreeList[FreeList.size() - 1];
					result->~T();
				}
				else {
					result = FreeList[lastRenewUnfinished];
				}

				try {
					new (result) T(std::forward<_Types>(_Args)...);
				}
				catch (...) {
					if (lastRenewUnfinished == NotUnfinished) {//检查是否需要标记，如果需要则就用下面的逻辑，不需要就按原样，等待下一次构造
						lastRenewUnfinished = FreeList.size() - 1;//标记renew失败的索引，让下一次重新构造的时候优先选择这个内存
					}
					throw;
				}

				if (lastRenewUnfinished != NotUnfinished) {
					//代表的是复用曾经构造异常的内存，这时候针对性移除元素
					//这里我们用一个巧妙的实现，来快速交换lastRenewUnfinished标记元素和尾部元素，实现删除的O1操作
					std::swap(FreeList[lastRenewUnfinished], FreeList[FreeList.size() - 1]);
					lastRenewUnfinished = NotUnfinished;//标记复用成功，即 没有未完成 状态
				}
				FreeList.pop_back();//将这段代码放到placement new之后，如果T构造函数异常了，则不弹出空闲列表
				return result;
			}
		}
		std::vector<T*> FreeList;
		std::stack<std::array<Block, OnePoolSize>> pool;
		std::shared_ptr<bool> IsDestruction = std::make_shared<bool>(false);
		size_t nextpos = 0;
		size_t lastRenewUnfinished = NotUnfinished;
	};
}
