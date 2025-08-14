#pragma once
#include <deque>
#include <array>
#include <memory>
#include <type_traits>
#include <functional>
#include <forward_list>
#include <new>
#include <utility>

namespace PinInCpp {
	//本质上是接管用不到的对象指针，在需要的时候重新构造/构造一个新的对象，如果你自己回收了也没问题，因为分配出去后权限归你
	template<typename T>
	class ObjectPtrPool {
	public:
		static_assert(!std::is_array_v<T>, "Cannot process c array");

		ObjectPtrPool() = default;

		ObjectPtrPool(const ObjectPtrPool&) = delete;//禁止复制
		ObjectPtrPool& operator=(const ObjectPtrPool&) = delete;

		ObjectPtrPool(ObjectPtrPool&&)noexcept = default;//允许移动
		ObjectPtrPool& operator=(ObjectPtrPool&)noexcept = default;

		~ObjectPtrPool() {
			ClearFreeList();
		}
		void ClearFreeList() {
			if (lastRenewUnfinished) {
				T* src = FreeList.back();
				::operator delete(src);//回收内存，但是要避免delete调用析构函数
				FreeList.pop_back();
				lastRenewUnfinished = false;
			}
			for (const auto ptr : FreeList) {
				delete ptr;
			}
			FreeList.clear();
		}
		//你需要将一个指针作为裸指针（比如调用release成员函数）传递进去，由对象池接管这个指针，析构函数会被ObjectPool自动调用，也就是你不用也不要调用析构函数
		//延迟析构的，只会在下一个对象需要分配时才析构这个对象（或者是ObjectPtrPool的ClearFreeList函数被调用了）
		void FreeToPool(T* ptr) {
			if (lastRenewUnfinished) {
				//如果是有异常状态的，则有一个已析构但未复用的对象存在队列末尾
				//那么我们需要一个巧妙的方法，去把末尾的元素一致放在最后面，实现异常安全
				T* last = FreeList.back();//先拷贝一份
				FreeList.back() = ptr;//新指针覆写旧指针
				FreeList.push_back(last);//把旧指针存到末尾，完成替换操作
			}
			else {//如果不是异常状态直接插入
				FreeList.push_back(ptr);
			}
		}
		template<typename... _Types>
		std::unique_ptr<T> NewObj(_Types&&..._Args) {
			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				return std::make_unique<T>(std::forward<_Types>(_Args)...);
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象，用placement new重新构造后转移所有权
				T* result = FreeList.back();
				if (!lastRenewUnfinished) {
					//这里有可能会被vs2022的静态分析报警告 "忽略函数返回值"，但是析构函数没有返回值，所以是误报
					result->~T();//因为ClearFreeList中的delete也会调用析构函数，所以这里延迟到这里调用
				}

				try {
					result = new (result) T(std::forward<_Types>(_Args)...);
				}
				catch (...) {
					lastRenewUnfinished = true;
					throw;
				}

				if (lastRenewUnfinished) {
					lastRenewUnfinished = false;
				}
				FreeList.pop_back();
				return std::unique_ptr<T>(result);//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
			}
		}

		//创建一个独占所有权的智能指针
		//自定义回收器传入的this指针是有效的对象，他没有被调用析构函数
		//自定义回收器的函数签名是 void(T* this, _Types...)，这个是接受this和构造参数一致的签名
		//也可以是 void(T* this) 这个签名也是合法的，区别是不会传入构造参数
		//自定义回收器本身能被以上形式调用即可，不关心他的来源类型
		//自定义回收器本身应该保证异常安全，抛出异常后不破坏原本的类
		template<typename... _Types>
		std::unique_ptr<T> NewObjCustomRecycle(auto& RecycleFn, _Types&&..._Args) {
			static_assert(std::is_invocable_v<decltype(RecycleFn), T*> || std::is_invocable_v<decltype(RecycleFn), T*, _Types...>, "RecycleFn is not a function / function signature is illegal");

			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				return std::make_unique<T>(std::forward<_Types>(_Args)...);
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象，用placement new重新构造后转移所有权
				T* result;
				result = FreeList.back();
				if (!lastRenewUnfinished) {//如果没有异常状态，则进入自定义回收流程
					if constexpr (std::is_invocable_v<decltype(RecycleFn), T*>) {
						RecycleFn(result);
					}
					else {
						RecycleFn(result, std::forward<_Types>(_Args)...);
					}
					//因为没有析构流程，所以抛出异常后还是安全的
				}
				else {//有异常状态，则用placement new
					new (result) T(std::forward<_Types>(_Args)...);
					lastRenewUnfinished = false;
				}
				FreeList.pop_back();//将这段代码放到placement new之后，如果T构造函数异常了，则不弹出空闲列表
				return std::unique_ptr<T>(result);
			}
		}
	private:
		std::deque<T*> FreeList;
		bool lastRenewUnfinished = false;
	};
	/* 目前不打算维护这个组件
	//内存池+对象池机制，快速方便的池化对象和内存分配
	//回收对象采用延迟析构模式，只有在对象池本身被析构了/从空闲列表上分配新对象了操作析构应该被析构的对象
	template<typename T, size_t OnePoolSize, typename base = T>
	class ObjectPool {
	public:
		static_assert(!std::is_array_v<T>, "Cannot process c array");
		static_assert(OnePoolSize, "pool size cannot be 0");
		static_assert(std::is_same_v<T, base> || std::is_base_of_v<base, T>, "The base class must be the base class of T/T");

		ObjectPool() {
			PushNewBlock();//压入一块内存
		}
		~ObjectPool() {
			TrueClearMemoryPool();
		}
		ObjectPool(const ObjectPool&) = delete;
		ObjectPool& operator=(const ObjectPool&) = delete;

		ObjectPool(ObjectPool&&) = delete;
		ObjectPool& operator=(ObjectPool&&) = delete;

		template<typename... _Types>
		std::unique_ptr<T, std::function<void(base*)>> MakeUnique(_Types&&..._Args) {//创建一个独占所有权的智能指针
			return MakeSmartPtrHasDeleter<std::unique_ptr<T, std::function<void(base*)>>>(NewObj(std::forward<_Types>(_Args)...));//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
		}

		template<typename... _Types>
		std::shared_ptr<T> MakeShared(_Types&&..._Args) {//创建一个共享所有权的智能指针
			return MakeSmartPtrHasDeleter<std::shared_ptr<T>>(NewObj(std::forward<_Types>(_Args)...));//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
		}

		//创建一个独占所有权的智能指针
		//自定义回收器传入的this指针是有效的对象，他没有被调用析构函数
		//自定义回收器的函数签名是 void(T* this, _Types...)，这个是接受this和构造参数一致的签名
		//也可以是 void(T* this) 这个签名也是合法的，区别是不会传入构造参数
		//自定义回收器本身能被以上形式调用即可，不关心他的来源类型
		//自定义回收器本身应该保证异常安全，抛出异常后不破坏原本的类
		template<typename... _Types>
		std::unique_ptr<T, std::function<void(base*)>> MakeUniqueCustomRecycle(auto& RecycleFn, _Types&&..._Args) {
			return MakeSmartPtrHasDeleter<std::unique_ptr<T, std::function<void(base*)>>>(NewObjCustomRecycle(RecycleFn, std::forward<_Types>(_Args)...));//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
		}

		//创建一个共享所有权的智能指针
		//自定义回收器传入的this指针是有效的对象，他没有被调用析构函数
		//自定义回收器的函数签名是 void(T* this, _Types...)，这个是接受this和构造参数一致的签名
		//也可以是 void(T* this) 这个签名也是合法的，区别是不会传入构造参数
		//自定义回收器本身能被以上形式调用即可，不关心他的来源类型
		//自定义回收器本身应该保证异常安全，抛出异常后不破坏原本的类
		template<typename... _Types>
		std::shared_ptr<T> MakeSharedCustomRecycle(auto& RecycleFn, _Types&&..._Args) {//创建一个共享所有权的智能指针
			return MakeSmartPtrHasDeleter<std::shared_ptr<T>>(NewObjCustomRecycle(RecycleFn, std::forward<_Types>(_Args)...));//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
		}

		//创建一个空的，但绑定好了删除器的unique_ptr
		std::unique_ptr<T, std::function<void(base*)>> MakeUniqueNullHasDeleter() {
			return MakeSmartPtrHasDeleter<std::unique_ptr<T, std::function<void(base*)>>>();
		}

		//创建一个空的，但绑定好了删除器的shared_ptr
		std::shared_ptr<T> MakeSharedNullHasDeleter() {
			return MakeSmartPtrHasDeleter<std::shared_ptr<T>>();
		}

		//当前分配出去的对象数量
		size_t size()const noexcept {
			if (pool.empty()) {
				return 0;
			}
			return (poolSize - 1) * OnePoolSize + nextpos - FreeList.size();
		}
		//单池容量
		constexpr size_t GetOnePoolSize()const noexcept {
			return OnePoolSize;
		}
		//实际占用大小
		size_t PoolCapacity() const noexcept {
			return poolSize * OnePoolSize;
		}
		//池的数量
		size_t PoolCount()const noexcept {
			return poolSize;
		}
		//警告！！！这个api是用于精细管理内存的
		//如果你想抛弃你之前分配的所有对象，让他们的生命周期立刻结束，调用这个能完成你想要的这个操作
		//随后你可以继续用NewObj新建对象，对象池会申请一块新的内存用于分配
		//如果你持有调用这个前由对象池分配的智能指针，调用此函数前记得先释放，当然，完成后也可以，我用共享所有权的智能指针确保了安全性
		void ClearAllMemoryPool() {
			TrueClearMemoryPool();
			FreeList.clear();//清空空闲列表

			IsDestruction = std::make_shared<bool>(false);//新的指针
			lastRenewUnfinished = false;//重置可能的异常状态
			poolSize = 0;//重置大小
		}
		//尝试清理空闲块，注意！这可能是一个很耗时的操作
		void ShrinkToFit() {
			size_t FreeListSize = FreeList.size();
			if (FreeListSize < nextpos) {//如果元素不满足一个块内分配的对象大小
				return;
			}
			size_t pos;
			T* lastRenewPtr = lastRenewUnfinished ? FreeList.back() : nullptr;//标记析构了但没复用成功的

			auto before = pool.before_begin();
			auto current = pool.begin();

			bool isHead = true;
			while (current != pool.end()) {
				pos = 0;
				std::array<Block, OnePoolSize>& blocks = *current;
				T* BlocksStart = reinterpret_cast<T*>(blocks.data());
				T* BlocksEnd = reinterpret_cast<T*>(blocks.data() + OnePoolSize);
				for (const auto& ptr : FreeList) {//遍历空闲列表，寻找有多少个位于blocks范围内的指针
					if (ptr >= BlocksStart && ptr < BlocksEnd) {
						pos++;
						if (pos == OnePoolSize) {
							break;
						}
					}
				}
				if ((isHead && pos == nextpos) || pos == OnePoolSize) {
					for (size_t i = 0; i < pos; i++) {
						T* tmp = BlocksStart + i;
						if (tmp != lastRenewPtr) {//避免重复析构
							tmp->~T();
						}
						else {
							lastRenewUnfinished = false;//如果真有，那么就重置异常状态
						}
					}
					std::erase_if(FreeList, [BlocksStart, BlocksEnd](T* ptr) {
						return (ptr >= BlocksStart && ptr < BlocksEnd);
						});//如果在范围内，则移除

					current = pool.erase_after(before);
					poolSize--;
					if (isHead) {//如果真的是头部被清理掉了
						nextpos = OnePoolSize;//让下一次能分配
						//因此不需要在外部判断是否为空，因为只有头部被清空的时候才需要下一次分配，那么清空本质上也是头部被清理掉了
					}
				}
				else {
					before = current;
					++current;
				}
				isHead = false;
			}
		}
	private:
		struct Block {
			alignas(T) std::byte b[sizeof(T)];
		};
		void FreeToPool(T* ptr) {
			if (lastRenewUnfinished) {
				//如果是有异常状态的，则有一个已析构但未复用的对象存在队列末尾
				//那么我们需要一个巧妙的方法，去把末尾的元素一致放在最后面，实现异常安全
				T* last = FreeList.back();//先拷贝一份
				FreeList.back() = ptr;//新指针覆写旧指针
				FreeList.push_back(last);//把旧指针存到末尾，完成替换操作
			}
			else {//如果不是异常状态直接插入
				FreeList.push_back(ptr);
			}
		}

		template<typename retv>
		retv MakeSmartPtrHasDeleter(T* ptr = nullptr) {
			std::weak_ptr<bool> IsDestructionWeak = IsDestruction;//先把所有权共享给函数局部
			auto delFn = [this, IsDestructionWeak](base* ptr) {//传递共享所有权的智能指针进去，确保他能被通知自己管理的内存是否被析构了
				if (!IsDestructionWeak.expired()) {//如果没有被析构
					this->FreeToPool((T*)ptr);
				}
				};
			return retv(ptr, delFn);
		}

		void TrueClearMemoryPool() {
			if (pool.empty()) {
				return;
			}
			IsDestruction = nullptr;//释放内存，std::weak_ptr可以得知观察的指针不存在了，让禁止智能指针调用FreeToPool
			T* lastRenewPtr = lastRenewUnfinished ? FreeList.back() : nullptr;//标记析构了但没复用成功的

			while (!pool.empty()) {//因为采用延迟析构实现，所以空闲列表中的指针都不会被真正的析构，只有在分配出去后/这里才会析构
				std::array<Block, OnePoolSize>& arr = pool.front();
				for (size_t i = 0; i < nextpos; i++) {
					T* tmp = reinterpret_cast<T*>(arr.data() + i);
					if (tmp != lastRenewPtr) {//避免重复析构
						tmp->~T();
					}
				}
				nextpos = OnePoolSize;//如果下一次还有，则从下一次的顶部开始指定，结束后也是置顶的，确保潜在的下一次分配生效
				pool.pop_front();
			}//析构函数手动执行完成后，剩下的内存块就可以安心交给STL
		}

		template<typename... _Types>
		T* NewObj(_Types&&..._Args) {//目标是尝试构造一个对象并返回其裸指针
			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				T* result = GetPoolNewPtr();
				new (result) T(std::forward<_Types>(_Args)...);
				nextpos++;//标记位移动延迟到构造完成，如果构造函数抛出异常，则相当于没移动，下一次依旧可以用，如果没有异常则正常移动，避免try块的设计
				return result;
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象，用placement new重新构造后转移所有权
				T* result;
				result = FreeList.back();
				if (!lastRenewUnfinished) {//如果没有异常状态，则调用析构函数
					//这里有可能会被vs2022的静态分析报警告 "忽略函数返回值"，但是析构函数没有返回值，所以是误报
					//延迟到这里析构，主要是方便ObjectPool的析构函数实现
					result->~T();
				}

				try {
					result = new (result) T(std::forward<_Types>(_Args)...);
				}
				catch (...) {
					lastRenewUnfinished = true;
					throw;
				}

				if (lastRenewUnfinished) {
					lastRenewUnfinished = false;//标记复用成功，即 没有未完成 状态
				}
				FreeList.pop_back();//将这段代码放到placement new之后，如果T构造函数异常了，则不弹出空闲列表
				return result;
			}
		}

		template<typename... _Types>
		T* NewObjCustomRecycle(auto& fn, _Types&&..._Args) {
			static_assert(std::is_invocable_v<decltype(fn), T*> || std::is_invocable_v<decltype(fn), T*, _Types...>, "RecycleFn is not a function / function signature is illegal");

			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				T* result = GetPoolNewPtr();
				new (result) T(std::forward<_Types>(_Args)...);
				nextpos++;//标记位移动延迟到构造完成，如果构造函数抛出异常，则相当于没移动，下一次依旧可以用，如果没有异常则正常移动，避免try块的设计
				return result;
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象
				T* result;
				result = FreeList.back();
				if (!lastRenewUnfinished) {//如果没有异常状态，则进入自定义回收流程
					if constexpr (std::is_invocable_v<decltype(fn), T*>) {
						fn(result);
					}
					else {
						fn(result, std::forward<_Types>(_Args)...);
					}
					//因为没有析构流程，所以抛出异常后还是安全的
				}
				else {//有异常状态，则用placement new
					result = new (result) T(std::forward<_Types>(_Args)...);
					lastRenewUnfinished = false;
				}
				FreeList.pop_back();//将这段代码放到placement new之后，如果T构造函数异常了，则不弹出空闲列表
				return result;
			}
		}
		T* GetPoolNewPtr() {
			if (nextpos >= OnePoolSize) {//如果没有空间了，就分配新的一块并重置nextpos
				PushNewBlock();//压入一块新内存
				nextpos = 0;
			}
			std::array<Block, OnePoolSize>& arr = pool.front();
			T* result = reinterpret_cast<T*>(arr.data() + nextpos);
			return result;
		}
		void PushNewBlock() {
			pool.emplace_front();
			poolSize++;
		}
		std::deque<T*> FreeList;
		std::forward_list<std::array<Block, OnePoolSize>> pool;
		size_t poolSize = 0;
		std::shared_ptr<bool> IsDestruction = std::make_shared<bool>(false);
		size_t nextpos = 0;
		bool lastRenewUnfinished = false;
	};*/
}
