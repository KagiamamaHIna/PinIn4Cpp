#pragma once
#include <vector>
#include <memory>
#include <type_traits>

namespace PinInCpp {
	//本质上是接管用不到的对象指针，在需要的时候重新构造/构造一个新的对象，如果你自己回收了也没问题，因为分配出去后权限归你
	template<typename T>
	class ObjectPool {
	public:
		static_assert(!std::is_array_v<T>, "Cannot process array");

		ObjectPool() = default;
		ObjectPool(const ObjectPool&) = delete;
		~ObjectPool() {
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
		//延迟析构的，只会在下一个对象需要分配时才析构这个对象（或者是ObjectPool的ClearFreeList函数被调用了）
		void FreeToPool(T* ptr) {
			FreeList.push_back(ptr);
		}
		template<typename... _Types>
		std::unique_ptr<T> NewObj(_Types&&..._Args) {
			if (FreeList.empty()) {//如果对象池空闲，那么就新建
				return std::make_unique<T>(_Args...);
			}
			else {//不空闲，就从对象池中取一个标记为要析构的对象，用placement new重新构造后转移所有权
				std::unique_ptr<T> result(FreeList[FreeList.size() - 1]);
				//这里有可能会被vs2022的静态分析报警告 "忽略函数返回值"，但是析构函数没有返回值，所以是误报
				result->~T();//因为ClearFreeList中的delete也会调用析构函数，所以这里进行延迟调用
				FreeList.pop_back();
				new (result.get()) T(_Args...);
				return result;//通过RVO/移动构造之类的形式，转移这个智能指针的所有权
			}
		}
	private:
		std::vector<T*> FreeList;
	};
}
