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
	namespace detail {
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

		template<typename T>
		struct OnlyDestruction {
			constexpr OnlyDestruction() noexcept = default;

			void operator()(T* ptr) const noexcept {//不会真的给你移除了
				ptr->~T();//但是会调用析构函数
			}
		};

		//仅调用析构函数的智能指针，可以用于slab分配器分配的对象管理
		template<typename T>
		using SlabUniqueObj = std::unique_ptr<T, OnlyDestruction<T>>;

		//纯粹的内存分配器，不管理其中对象的生命周期，需要手动回收
		template<typename UnitType, size_t ChunkSize>
		class SlabAllocator {
		public:
			static_assert(ChunkSize, "Chunk size cannot be 0");

			SlabAllocator() = default;

			SlabAllocator(const SlabAllocator&) = delete;//禁止复制
			SlabAllocator& operator=(const SlabAllocator&) = delete;

			SlabAllocator(SlabAllocator&& src)noexcept {
				data = src.data;
				head = src.head;
				src.data = nullptr;
				src.head = nullptr;
			}
			SlabAllocator& operator=(SlabAllocator&& src)noexcept {
				if (this == &src) {
					return *this;
				}
				data = src.data;
				head = src.head;
				src.data = nullptr;
				src.head = nullptr;
				return *this;
			}

			~SlabAllocator() {
				NodeBody* curr = data;
				while (curr != nullptr) {
					NodeBody* tmp = curr->next;
					delete curr;
					curr = tmp;
				}
			}

			std::byte* Alloc() {
				checkAndNewChunk();
				Node* result = head;
				head = head->next;
				return result->srcValue;
			}
			void FreeMem(std::byte* memoryData) {
				Node* nodeData = reinterpret_cast<Node*>(memoryData);//将byte重解释为Node节点
				nodeData->next = head;//设置next为活动成员，指向原先的头
				head = nodeData;//free的成为新头
			}

			template<typename... Types>
			UnitType* NewObj(Types&&...Args) {
				std::byte* srcBytes = Alloc();
				try {
					return new (reinterpret_cast<UnitType*>(srcBytes)) UnitType(std::forward<Types>(Args)...);
				}
				catch (...) {
					FreeMem(srcBytes);
					throw;
				}
			}

			void FreeObj(UnitType* obj) {
				obj->~UnitType();
				FreeMem(reinterpret_cast<std::byte*>(obj));
			}
		private:
			void checkAndNewChunk() {
				if (head == nullptr) {
					NodeBody* chunkBody = new NodeBody;//申请新的body
					chunkBody->next = data;//然后将新的next指向旧的data
					data = chunkBody;//再把新的设为data，用链表串联起来

					Node* newNodes = data->body;
					for (size_t i = 0; i < ChunkSize; i++) {
						newNodes[i].next = head;
						head = &newNodes[i];
					}
				}
			}

			using Chunk = std::byte[sizeof(UnitType)];

			union Node {
				alignas(UnitType) Chunk srcValue;//原始内存值
				Node* next;//指向下一个侵入式链表节点的指针
			};

			struct NodeBody {
				Node body[ChunkSize];
				NodeBody* next;
			};

			NodeBody* data = nullptr;//需要一个东西去管理已申请的内存块，单向链表就是最优解！
			Node* head = nullptr;
		};
	}
}
