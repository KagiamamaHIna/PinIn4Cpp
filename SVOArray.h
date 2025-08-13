#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace PinInCpp {
	//一个特化组件，实现一个通用的SVO容器有点太麻烦了，不考虑删除，但是支持清空
	template<typename T, uint8_t __size>
	class SVOArray {
	public:
		static_assert(__size, "small vector size cannot be 0");

		SVOArray() {
			data.shortData = {};
		}
		SVOArray(const SVOArray& src) {
			copy(src);
		}
		SVOArray(SVOArray&& src)noexcept {
			move(std::forward<SVOArray&&>(src));//确保类型绝对正确
		}
		SVOArray& operator=(const SVOArray& src) {
			if (this == &src) {
				return *this;
			}
			copy(src);
			return *this;
		}
		SVOArray& operator=(SVOArray&& src)noexcept {
			if (this == &src) {
				return *this;
			}
			move(std::forward<SVOArray&&>(src));//确保类型绝对正确
			return *this;
		}
		~SVOArray() {
			TrueClear();
		}
		void clear() {
			TrueClear();
			dataSize = 0;
			data.shortData = {};
		}
		bool empty()const noexcept {
			return dataSize == 0;
		}
		size_t size()const noexcept {
			return dataSize;
		}
		T& operator[](size_t key)noexcept {
			if (dataSize > __size) {
				return *reinterpret_cast<T*>(data.longData.data + key);
			}
			return *reinterpret_cast<T*>(data.shortData.data + key);
		}
		const T& operator[](size_t key)const noexcept {
			if (dataSize > __size) {
				return *reinterpret_cast<const T*>(data.longData.data + key);
			}
			return *reinterpret_cast<const T*>(data.shortData.data + key);
		}

		T& at(size_t key) {
			if (key >= dataSize) {
				throw std::out_of_range("invalid vector subscript");
			}
			if (dataSize > __size) {
				return *reinterpret_cast<T*>(data.longData.data + key);
			}
			return *reinterpret_cast<T*>(data.shortData.data + key);
		}
		const T& at(size_t key)const {
			if (key >= dataSize) {
				throw std::out_of_range("invalid vector subscript");
			}
			if (dataSize > __size) {
				return *reinterpret_cast<const T*>(data.longData.data + key);
			}
			return *reinterpret_cast<const T*>(data.shortData.data + key);
		}

		template<typename... _Types>
		void emplace_back(_Types&&..._Args) {
			if (dataSize < __size) {//小向量情况
				T* newElem = reinterpret_cast<T*>(data.shortData.data + dataSize);
				new (newElem) T(std::forward<_Types>(_Args)...);
				dataSize++;
			}
			else if (dataSize == __size) {//转换为大向量情况
				//要考虑扩容因子
				T temp(std::forward<_Types>(_Args)...);//构造一个栈上临时对象
				//如果抛出异常了，则后续什么都不做

				T* Src = reinterpret_cast<T*>(data.shortData.data);
				size_t cap = dataSize * 2;
				Chunk* newChunk = new Chunk[cap];//申请缓冲区，假设移动构造不抛异常
				for (size_t i = 0; i < __size; i++) {
					new (reinterpret_cast<T*>(newChunk + i)) T(std::move(Src[i]));//调用移动构造函数移动资源
					Src[i].~T();//析构掉数据
				}
				new (reinterpret_cast<T*>(newChunk + __size)) T(std::move(temp));//移动临时对象
				dataSize++;
				data.longData.cap = cap;
				data.longData.data = newChunk;
			}
			else {//大向量情况
				if (dataSize >= data.longData.cap) {
					size_t cap = dataSize * 2;
					T* Src = reinterpret_cast<T*>(data.longData.data);
					Chunk* newChunk = new Chunk[cap];//申请缓冲区，假设移动构造不抛异常
					for (size_t i = 0; i < dataSize; i++) {
						new (reinterpret_cast<T*>(newChunk + i)) T(std::move(Src[i]));//调用移动构造函数移动资源
						Src[i].~T();//析构掉数据
					}
					delete[] data.longData.data;//回收旧缓冲区
					data.longData.cap = cap;//更新cap，因为这个是根据容量确定的，所以后续如果抛出异常了dataSize没自增，那么也不会再次进入扩容判断
					data.longData.data = newChunk;
				}
				new (reinterpret_cast<T*>(data.longData.data + dataSize)) T(std::forward<_Types>(_Args)...);
				dataSize++;
			}
		}
		const T* begin()const noexcept {
			if (dataSize > __size) {
				return reinterpret_cast<const T*>(data.longData.data);
			}
			return reinterpret_cast<const T*>(data.shortData.data);
		}
		T* begin()noexcept {
			if (dataSize > __size) {
				return reinterpret_cast<T*>(data.longData.data);
			}
			return reinterpret_cast<T*>(data.shortData.data);
		}
		const T* end()const noexcept {
			if (dataSize > __size) {
				return reinterpret_cast<const T*>(data.longData.data + dataSize);
			}
			return reinterpret_cast<const T*>(data.shortData.data + dataSize);
		}
		T* end()noexcept {
			if (dataSize > __size) {
				return reinterpret_cast<T*>(data.longData.data + dataSize);
			}
			return reinterpret_cast<T*>(data.shortData.data + dataSize);
		}
	private:
		void TrueClear() {
			if (dataSize > __size) {//只有大于的情况才代表变成大向量了
				for (size_t i = 0; i < dataSize; i++) {
					reinterpret_cast<T*>(data.longData.data + i)->~T();
				}
				delete[] data.longData.data;
			}
			else {
				for (size_t i = 0; i < dataSize; i++) {
					reinterpret_cast<T*>(data.shortData.data + i)->~T();
				}
			}
		}
		void copy(const SVOArray& src) {
			const T* SrcElem;
			T* TargetElem;

			dataSize = src.dataSize;
			if (dataSize > __size) {
				data.longData.cap = src.data.longData.cap;
				data.longData.data = new Chunk[data.longData.cap];
				SrcElem = reinterpret_cast<const T*>(src.data.longData.data);
				TargetElem = reinterpret_cast<T*>(data.longData.data);
			}
			else {
				data.shortData = {};
				SrcElem = reinterpret_cast<const T*>(src.data.shortData.data);
				TargetElem = reinterpret_cast<T*>(data.shortData.data);
			}
			size_t i = 0;
			try {
				for (; i < dataSize; i++) {
					new (TargetElem + i) T(*(SrcElem + i));
				}
			}
			catch (...) {
				//如果复制构造抛出异常了，我们得手动调用已经构造完的元素的析构函数
				//当第i个元素抛出异常的时候，那么第i个元素是没有成功构造的，所以不用析构第i个元素
				for (size_t j = 0; j < i; j++) {
					TargetElem[j].~T();
				}
				if (dataSize > __size) {
					delete[] data.longData.data;
				}
				throw;
			}
		}
		void move(SVOArray&& src)noexcept {
			dataSize = src.dataSize;
			if (dataSize > __size) {//大向量指针转移
				data.longData.cap = src.data.longData.cap;
				data.longData.data = src.data.longData.data;
			}
			else {//小向量需要数据转移
				data.shortData = {};
				T* SrcElem = reinterpret_cast<T*>(src.data.shortData.data);
				T* TargetElem = reinterpret_cast<T*>(data.shortData.data);
				for (size_t i = 0; i < dataSize; i++) {
					new (TargetElem + i) T(std::move(SrcElem[i]));
					SrcElem[i].~T();
				}
			}
			src.data.shortData = {};//清空原始数据
			src.dataSize = 0;
		}
		struct Chunk {
			alignas(T) std::byte b[sizeof(T)];
		};
		struct ShortUint {
			Chunk data[__size];
		};
		struct LongUint {
			size_t cap;
			Chunk* data;
		};
		union DataUnion {
			ShortUint shortData;
			LongUint longData;
		};

		size_t dataSize = 0;//容器的大小，同时是下一个分配的位置
		DataUnion data;
	};
}
