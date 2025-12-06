#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <new>
#include <utility>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>

namespace PinInCpp {
	namespace detail {
		//一个特化组件，实现一个通用的SVO容器有点太麻烦了，不考虑删除，但是支持清空
		template<typename T, uint8_t SVOsize>
		class SVOArray {
		public:
			static_assert(SVOsize, "small vector size cannot be 0");

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
				if (dataSize > SVOsize) {
					return *reinterpret_cast<T*>(data.longData.data[key]);
				}
				return *reinterpret_cast<T*>(data.shortData.data[key]);
			}
			const T& operator[](size_t key)const noexcept {
				if (dataSize > SVOsize) {
					return *reinterpret_cast<const T*>(data.longData.data[key]);
				}
				return *reinterpret_cast<const T*>(data.shortData.data[key]);
			}

			T& at(size_t key) {
				if (key >= dataSize) {
					throw std::out_of_range("invalid vector subscript");
				}
				if (dataSize > SVOsize) {
					return *reinterpret_cast<T*>(data.longData.data[key]);
				}
				return *reinterpret_cast<T*>(data.shortData.data[key]);
			}
			const T& at(size_t key)const {
				if (key >= dataSize) {
					throw std::out_of_range("invalid vector subscript");
				}
				if (dataSize > SVOsize) {
					return *reinterpret_cast<const T*>(data.longData.data[key]);
				}
				return *reinterpret_cast<const T*>(data.shortData.data[key]);
			}

			template<typename... _Types>
			void emplace_back(_Types&&..._Args) {
				if (dataSize < SVOsize) {//小向量情况
					T* newElem = reinterpret_cast<T*>(data.shortData.data[dataSize]);
					new (newElem) T(std::forward<_Types>(_Args)...);
					dataSize++;
				}
				else if (dataSize == SVOsize) {//转换为大向量情况
					//要考虑扩容因子
					T temp(std::forward<_Types>(_Args)...);//构造一个栈上临时对象
					//如果抛出异常了，则后续什么都不做

					T* Src = reinterpret_cast<T*>(data.shortData.data);
					constexpr size_t cap = SVOsize * 2;
					Chunk* newChunk = reinterpret_cast<Chunk*>(::operator new (sizeof(Chunk) * cap, HeapAlign));//申请缓冲区，假设移动构造不抛异常
					for (size_t i = 0; i < SVOsize; i++) {
						new (reinterpret_cast<T*>(newChunk[i])) T(std::move(Src[i]));//调用移动构造函数移动资源
						Src[i].~T();//析构掉数据
					}
					new (reinterpret_cast<T*>(newChunk[SVOsize])) T(std::move(temp));//移动临时对象
					dataSize++;
					data.longData.cap = cap;
					data.longData.data = newChunk;
				}
				else {//大向量情况
					if (dataSize >= data.longData.cap) {
						size_t cap = dataSize * 2;
						T* Src = reinterpret_cast<T*>(data.longData.data);
						Chunk* newChunk = reinterpret_cast<Chunk*>(::operator new (sizeof(Chunk) * cap, HeapAlign));//申请缓冲区，假设移动构造不抛异常
						for (size_t i = 0; i < dataSize; i++) {
							new (reinterpret_cast<T*>(newChunk[i])) T(std::move(Src[i]));//调用移动构造函数移动资源
							Src[i].~T();//析构掉数据
						}
						::operator delete (data.longData.data, HeapAlign);//回收旧缓冲区
						data.longData.cap = cap;//更新cap，因为这个是根据容量确定的，所以后续如果抛出异常了dataSize没自增，那么也不会再次进入扩容判断
						data.longData.data = newChunk;
					}
					new (reinterpret_cast<T*>(data.longData.data + dataSize)) T(std::forward<_Types>(_Args)...);
					dataSize++;
				}
			}
			const T* begin()const noexcept {
				if (dataSize > SVOsize) {
					return reinterpret_cast<const T*>(data.longData.data);
				}
				return reinterpret_cast<const T*>(data.shortData.data);
			}
			T* begin()noexcept {
				if (dataSize > SVOsize) {
					return reinterpret_cast<T*>(data.longData.data);
				}
				return reinterpret_cast<T*>(data.shortData.data);
			}
			const T* end()const noexcept {
				if (dataSize > SVOsize) {
					return reinterpret_cast<const T*>(data.longData.data + dataSize);
				}
				return reinterpret_cast<const T*>(data.shortData.data + dataSize);
			}
			T* end()noexcept {
				if (dataSize > SVOsize) {
					return reinterpret_cast<T*>(data.longData.data + dataSize);
				}
				return reinterpret_cast<T*>(data.shortData.data + dataSize);
			}
		private:
			void TrueClear() {
				if (dataSize > SVOsize) {//只有大于的情况才代表变成大向量了
					for (size_t i = 0; i < dataSize; i++) {
						reinterpret_cast<T*>(data.longData.data[i])->~T();
					}
					::operator delete (data.longData.data, HeapAlign);
				}
				else {
					for (size_t i = 0; i < dataSize; i++) {
						reinterpret_cast<T*>(data.shortData.data[i])->~T();
					}
				}
			}
			void copy(const SVOArray& src) {
				const T* SrcElem;
				T* TargetElem;

				dataSize = src.dataSize;
				if (dataSize > SVOsize) {
					data.longData.cap = src.data.longData.cap;
					data.longData.data = reinterpret_cast<Chunk*>(::operator new (sizeof(Chunk) * data.longData.cap, HeapAlign));
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
					if (dataSize > SVOsize) {
						::operator delete (data.longData.data, HeapAlign);
					}
					throw;
				}
			}
			void move(SVOArray&& src)noexcept {
				dataSize = src.dataSize;
				if (dataSize > SVOsize) {//大向量指针转移
					data.longData.cap = src.data.longData.cap;
					data.longData.data = src.data.longData.data;
				}
				else {//小向量需要数据转移
					data.shortData = {};
					T* SrcElem = reinterpret_cast<T*>(src.data.shortData.data);
					T* TargetElem = reinterpret_cast<T*>(data.shortData.data);
					for (size_t i = 0; i < dataSize; i++) {
						new (TargetElem[i]) T(std::move(SrcElem[i]));
						SrcElem[i].~T();
					}
				}
				src.data.shortData = {};//清空原始数据
				src.dataSize = 0;
			}
			using Chunk = std::byte[sizeof(T)];
			struct ShortUnit {
				alignas(T) Chunk data[SVOsize];
			};
			struct LongUnit {
				size_t cap;
				Chunk* data;
			};
			union DataUnion {
				ShortUnit shortData;
				LongUnit longData;
			};
			static constexpr std::align_val_t HeapAlign = std::align_val_t(alignof(T));

			size_t dataSize = 0;//容器的大小，同时是下一个分配的位置
			DataUnion data;
		};

		template<typename value, bool lazyLoad = false>
		class AdaptiveSet {//自适应集合，小向量缓冲区->动态数组->哈希集合三级优化。特化组件，目前不考虑删除
		private:
			//AdaptiveSet转换临界点
			constexpr static size_t ContainerThreshold = 128;

			static constexpr uint8_t SVONum = sizeof(std::vector<value>) / sizeof(value);//计算一个合适的长度
			static constexpr bool isSVO = SVONum > 0;//计算是否可以无损SVO优化
			using ObjVector = std::conditional_t<isSVO, SVOArray<value, SVONum>, std::vector<value>>;

			class AbstractSet {//集合不承担查找，这个就很简单
			public:
				virtual ~AbstractSet() = default;
				virtual AbstractSet* insert(const value& input_v) = 0;
				virtual void DeserializationInsert(const value& input_v) = 0;
				virtual void AddToSTLSet(std::unordered_set<value>& input_v) = 0;//有点反客为主了
				virtual size_t size()const noexcept = 0;
				virtual void forEach(std::function<void(const value&)>& fn)const = 0;
			};
			class HashSet : public AbstractSet {
			public:
				virtual AbstractSet* insert(const value& input_v) {
					data.insert(input_v);
					return this;
				}
				virtual void DeserializationInsert(const value& input_v) {
					data.insert(input_v);
				}
				virtual void AddToSTLSet(std::unordered_set<value>& input_v) {
					for (const value& v : data) {
						input_v.insert(v);
					}
				}
				virtual size_t size()const noexcept {
					return data.size();
				}
				virtual void forEach(std::function<void(const value&)>& fn)const {
					for (const auto& v : data) {
						fn(v);
					}
				}
			private:
				std::unordered_set<value> data;
			};
			class ArraySet : public AbstractSet {
			public:
				virtual AbstractSet* insert(const value& input_v) {
					for (const value& v : data) {
						if (v == input_v) {
							return this;//如果查到，有相等的，则为重复
						}
					}
					//置前检查，避免潜在的不必要堆分配
					if (data.size() + 1 > ContainerThreshold) {
						std::unique_ptr<HashSet> result = std::make_unique<HashSet>();
						for (value& v : data) {
							result->insert(std::move(v));
						}
						result->insert(input_v);
						return result.release();
					}
					if constexpr (!isSVO) {
						size_t capacity = data.capacity();
						if (data.size() + 1 > capacity) {//手动控制扩容因子，获取最高效的性能表现
							if (capacity == 0) {
								data.reserve(1);
							}
							else {
								data.reserve(capacity * 2);
							}
						}
					}
					data.emplace_back(input_v);
					return this;
				}
				virtual void DeserializationInsert(const value& input_v) {
					data.emplace_back(input_v);
				}
				virtual void AddToSTLSet(std::unordered_set<value>& input_v) {
					for (const value& v : data) {
						input_v.insert(v);
					}
				}
				virtual size_t size()const noexcept {
					return data.size();
				}
				virtual void forEach(std::function<void(const value&)>& fn)const {
					for (const auto& v : data) {
						fn(v);
					}
				}
				void reserve(size_t _Newcapacity) {
					if constexpr (!isSVO) {
						data.reserve(_Newcapacity);
					}
				}
			private:
				ObjVector data;
			};
			std::unique_ptr<AbstractSet> Container = nullptr;
		public:
			AdaptiveSet() {
				if constexpr (!lazyLoad) {
					Container = std::make_unique<ArraySet>();
				}
			}
			AdaptiveSet(std::initializer_list<value> list) :Container{ std::make_unique<ArraySet>() } {
				for (const value& v : list) {
					insert(v);
				}
			}
			AdaptiveSet(size_t size) {
				if (size > ContainerThreshold) {
					Container = std::make_unique<HashSet>();
				}
				else {
					std::unique_ptr<ArraySet> temp = std::make_unique<ArraySet>();
					if constexpr (!isSVO) {
						temp->reserve(size);
					}
					Container = std::move(temp);
				}
			}
			void insert(const value& input_v) {
				if constexpr (lazyLoad) {
					if (Container == nullptr) {
						Container = std::make_unique<ArraySet>();
					}
				}
				AbstractSet* set = Container->insert(input_v);
				if (set != Container.get()) {
					Container.reset(set);
				}
			}
			void AddToSTLSet(std::unordered_set<value>& input_v) {
				if constexpr (lazyLoad) {
					if (Container == nullptr) {
						return;
					}
				}
				Container->AddToSTLSet(input_v);
			}
			size_t size()const noexcept {
				if constexpr (lazyLoad) {
					if (Container == nullptr) {
						return 0;
					}
				}
				return Container->size();
			}
			void forEach(std::function<void(const value&)> fn)const {
				if constexpr (lazyLoad) {
					if (Container == nullptr) {
						return;
					}
				}
				Container->forEach(fn);
			}
			void DeserializationInsert(const value& v) {//反序列化时用的接口，用于快速恢复状态，内部不会进行升级检查
				Container->DeserializationInsert(v);
			}
		};

		template<typename k, typename v>
		class AdaptiveMap {//自适应表，小向量缓冲区->动态数组->哈希表三级优化。特化组件，目前不考虑删除
		private:
			using KVPair = std::pair<const k, v>;
			//AdaptiveSet转换临界点
			constexpr static size_t ContainerThreshold = 128;
			static constexpr uint8_t SVONum = sizeof(std::vector<KVPair>) / sizeof(KVPair);//计算一个合适的长度
			static constexpr bool isSVO = SVONum > 0;//计算是否可以无损SVO优化
			using ObjVector = std::conditional_t<isSVO, SVOArray<KVPair, SVONum>, std::vector<KVPair>>;

			class AbstractMap {
			public:
				virtual ~AbstractMap() = default;
				virtual AbstractMap* insert_or_assign(const k& input_v, v&& value) = 0;
				virtual AbstractMap* get(const k&, v**) = 0;//要修改一个指针，使得其可以传递引用给调用方
				virtual size_t size()const noexcept = 0;
				virtual void forEach(std::function<void(const k&, const v&)>& fn)const = 0;
				virtual v* find(const k&) = 0;
				virtual void clear() = 0;
				virtual void DeserializationInsert(const k& input_v, v&& value) = 0;
			};
			class HashMap : public AbstractMap {
			public:
				HashMap() = default;
				virtual ~HashMap() = default;
				virtual AbstractMap* insert_or_assign(const k& input_v, v&& value) {
					data.insert_or_assign(input_v, std::move(value));
					return this;
				}
				virtual AbstractMap* get(const k& key, v** ptr) {
					(*ptr) = &data[key];
					return this;
				}
				virtual size_t size()const noexcept {
					return data.size();
				}
				virtual void forEach(std::function<void(const k&, const v&)>& fn)const {
					for (const auto& [key, value] : data) {
						fn(key, value);
					}
				}
				virtual v* find(const k& key) {
					auto it = data.find(key);
					if (it == data.end()) {
						return nullptr;
					}
					return &it->second;
				}
				virtual void clear() {
					data.clear();
				}
				void DeserializationInsert(const k& input_v, v&& value) {
					data.insert_or_assign(input_v, std::move(value));
				}
			private:
				std::unordered_map<k, v> data;
			};
			class ArrayMap : public AbstractMap {
			public:
				ArrayMap() = default;
				virtual ~ArrayMap() = default;
				virtual AbstractMap* insert_or_assign(const k& input_v, v&& value) {//不查重，内部契约决定了其不用查重
					if (data.size() + 1 > ContainerThreshold) {
						std::unique_ptr<HashMap> result = std::make_unique<HashMap>();
						for (auto& [data_key, data_value] : data) {
							result->insert_or_assign(data_key, std::move(data_value));
						}
						result->insert_or_assign(input_v, std::move(value));
						return result.release();
					}
					data.emplace_back(KVPair(input_v, std::move(value)));
					return this;
				}
				virtual AbstractMap* get(const k& key, v** ptr) {
					for (auto& p : data) {
						if (p.first == key) {
							(*ptr) = &p.second;
							return this;
						}
					}
					if (data.size() + 1 > ContainerThreshold) {//默认值机制，且触发升级
						std::unique_ptr<HashMap> result = std::make_unique<HashMap>();
						for (auto& [data_key, data_value] : data) {
							result->insert_or_assign(data_key, std::move(data_value));
						}
						result->get(key, ptr);
						return result.release();
					}

					if constexpr (!isSVO) {
						size_t capacity = data.capacity();
						if (data.size() + 1 > capacity) {//手动控制扩容因子，获取最高效的性能表现
							if (capacity == 0) {
								data.reserve(1);
							}
							else {
								data.reserve(capacity * 2);
							}
						}
					}

					data.emplace_back(KVPair(key, v()));//默认值机制，不触发升级
					KVPair& p = data[data.size() - 1];
					(*ptr) = &p.second;
					return this;
				}
				virtual size_t size()const noexcept {
					return data.size();
				}
				virtual void forEach(std::function<void(const k&, const v&)>& fn)const {
					for (const auto& [key, value] : data) {
						fn(key, value);
					}
				}
				virtual v* find(const k& key) {
					for (auto& p : data) {
						if (p.first == key) {
							return &p.second;
						}
					}
					return nullptr;
				}
				virtual void clear() {
					data.clear();
				}
				void reserve(size_t _Newcapacity) {
					if constexpr (!isSVO) {
						data.reserve(_Newcapacity);
					}
				}
				void DeserializationInsert(const k& input_v, v&& value) {
					data.emplace_back(KVPair(input_v, std::move(value)));
				}
			private:
				ObjVector data;
			};
			std::unique_ptr<AbstractMap> Container;
		public:
			AdaptiveMap() :Container{ std::make_unique<ArrayMap>() } {
			}
			AdaptiveMap(size_t size) {
				if (size > ContainerThreshold) {
					Container = std::make_unique<HashMap>();
				}
				else {
					std::unique_ptr<ArrayMap> temp = std::make_unique<ArrayMap>();
					if constexpr (!isSVO) {
						temp->reserve(size);
					}
					Container = std::move(temp);
				}
			}
			void insert_or_assign(const k& input_v, v&& value) {
				AbstractMap* temp = Container->insert_or_assign(input_v, std::move(value));
				if (temp != Container.get()) {
					Container.reset(temp);
				}
			}
			size_t size()const noexcept {
				return Container->size();
			}
			void forEach(std::function<void(const k&, const v&)> fn)const {
				Container->forEach(fn);
			}
			v& operator[](const k& key) {
				v* result;
				AbstractMap* temp = Container->get(key, &result);
				if (temp != Container.get()) {
					Container.reset(temp);
				}
				return *result;
			}
			v* find(const k& key) {//返回nullptr代表没找到，写迭代器还是太麻烦了！
				return Container->find(key);
			}
			void clear() {
				Container->clear();
			}
			void DeserializationInsert(const k& input_v, v&& value) {//反序列化时用的接口，用于快速恢复状态，内部不会进行升级检查
				Container->DeserializationInsert(input_v, std::move(value));
			}
		};
	}
}
