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
			T* lastRenewPtr = nullptr;
			if (lastRenewUnfinished) {//如果上一次析构完成但没有成功重分配的话，这个值是真
				lastRenewPtr = FreeList[FreeList.size() - 1];
			}
			while (!pool.empty()) {//因为采用延迟析构实现，所以空闲列表中的指针都不会被真正的析构，只有在分配出去后/这里才会析构
				std::array<Block, OnePoolSize>& arr = pool.top();
				for (size_t i = 0; i < nextpos; i++) {
					T* tmp = reinterpret_cast<T*>(arr.data() + i);
					if (tmp != lastRenewPtr) {//避免重复析构
						tmp->~T();
					}
				}
				nextpos = OnePoolSize;//如果下一次还有，则从下一次的顶部开始指定
				pool.pop();
			}//析构函数手动执行完成后，剩下的内存块就可以安心交给STL容器释放了
		}
		ObjectPool(const ObjectPool&) = delete;
		ObjectPool(ObjectPool&&) = delete;
		ObjectPool& operator=(ObjectPool&&) = delete;

		template<typename... _Types>
		std::unique_ptr<T, std::function<void(T*)>> NewObj(_Types&&..._Args) {
			auto delFn = [this](T* ptr) {this->FreeToPool(ptr); };
			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				if (nextpos >= OnePoolSize) {//如果没有空间了，就分配新的一块并重置nextpos
					pool.push(std::array<Block, OnePoolSize>());//压入一块新内存
					nextpos = 0;
				}
				std::array<Block, OnePoolSize>& arr = pool.top();
				T* result = reinterpret_cast<T*>(arr.data() + nextpos);
				new (result) T(std::forward<_Types>(_Args)...);
				nextpos++;//标记位移动延迟到构造完成，如果构造函数抛出异常，则相当于没移动，下一次依旧可以用，如果没有异常则正常移动，避免try块的设计
				return std::unique_ptr<T, std::function<void(T*)>>(result, delFn);//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象，用placement new重新构造后转移所有权
				T* result = FreeList[FreeList.size() - 1];
				if (!lastRenewUnfinished) {
					//这里有可能会被vs2022的静态分析报警告 "忽略函数返回值"，但是析构函数没有返回值，所以是误报
					//延迟到这里析构，主要是方便ObjectPool的析构函数实现
					result->~T();
				}
				try {
					new (result) T(std::forward<_Types>(_Args)...);
				}
				catch (...) {
					lastRenewUnfinished = true;//标记上一次Renew执行失败，下一次将不再尝试原本的析构T
					throw;
				}
				FreeList.pop_back();//将这段代码放到placement new之后，如果T构造函数异常了，则不弹出空闲列表
				lastRenewUnfinished = false;//标记析构成功
				return std::unique_ptr<T, std::function<void(T*)>>(result, delFn);//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
			}
		}
		void FreeListShrinkToFit() {
			FreeList.shrink_to_fit();
		}
		//警告！！！你应当只在准备要析构ObjectPool的时候才考虑设置这个，关闭空闲列表会导致分配新内存时发生分配器逻辑上的内存泄漏
		//如果你确定ObjectPool真的要被析构了，同时想让所有智能指针去析构时不要把自身管理的指针交给空闲列表时(提升性能)，手动关闭这个可以避免空闲列表的增长
		//因为延迟析构策略导致的，只有在ObjectPool被析构时，分配的对象才会被全部，完全的析构
		//所以会被析构的对象实际上和空闲列表完全无关！
		//因此，最后强调一遍，你应当只在准备要析构ObjectPool的时候才考虑设置这个(设置为false关闭)，原因如上。可以提升理论性能
		void SetFreeListEnable(bool enabled) {
			FreeListEnable = enabled;
		}
	private:
		struct Block {
			std::byte b[sizeof(T)];
		};
		void FreeToPool(T* ptr) {
			if (FreeListEnable) {
				FreeList.push_back(ptr);
			}
		}

		std::vector<T*> FreeList;
		std::stack<std::array<Block, OnePoolSize>> pool;
		size_t nextpos = 0;
		bool lastRenewUnfinished = false;
		bool FreeListEnable = true;
	};
}
