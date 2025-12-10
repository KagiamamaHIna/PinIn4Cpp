#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <unordered_map>
#include <array>

#include "PinIn.h"
#include "Keyboard.h"

#include "PinIn4Cpp/detail/StringPool.h"
#include "PinIn4Cpp/detail/Accelerator.h"
#include "PinIn4Cpp/detail/ObjectPool.h"
#include "PinIn4Cpp/detail/BinUtils.h"
#include "PinIn4Cpp/detail/AdaptiveContainers.h"

namespace PinInCpp {
	enum class Logic : uint8_t {//不需要很多状态的枚举类
		BEGIN, CONTAIN, EQUAL
	};

	class TreeSearcher {
	public:
		TreeSearcher(Logic logic, std::string_view PinyinDictionaryPath)
			:logic{ logic }, context(std::make_shared<PinIn>(PinyinDictionaryPath)), acc(*context) {
			init();
		}

		TreeSearcher(Logic logic, const std::vector<char>& PinyinDictionaryData)
			:logic{ logic }, context(std::make_shared<PinIn>(PinyinDictionaryData)), acc(*context) {
			init();
		}

		TreeSearcher(Logic logic, std::shared_ptr<PinIn> PinInShared)//如果你想共享一个PinIn对象，那么应该传递这个智能指针
			:logic{ logic }, context(PinInShared), acc(*context) {
			init();
		}
		~TreeSearcher() = default;

		//因为绑定着this指针，所以不能移动和拷贝
		TreeSearcher(const TreeSearcher&) = delete;
		TreeSearcher& operator=(const TreeSearcher&) = delete;
		TreeSearcher(TreeSearcher&&) = delete;
		TreeSearcher& operator=(TreeSearcher&&) = delete;

		//32位环境和64位环境生成的文件格式应该是通用的，但是如果64位下数据量过大，32位环境下加载有潜在的溢出导致逻辑错误的风险
		//你应该只传入对应类的Serialization方法生成的数据，传入一个不正确格式的很有可能会造成未定义行为等
		//不过有可能抛出std::out_of_range，抛出这个代表读取时越界了，通常意味着结构错误
		//抛出BinaryVersionInvalidException代表版本号错误，不可使用
		//请自行构建一个合适的PinIn类实例，PinIn类本身也可以序列化+反序列化
		static std::unique_ptr<TreeSearcher> Deserialize(const std::vector<uint8_t>& data, std::shared_ptr<PinIn> PinInShared, size_t index = 0);
		static std::optional<std::unique_ptr<TreeSearcher>> DeserializeFromFile(std::string_view path, std::shared_ptr<PinIn> PinInShared, size_t index = 0) {
			std::optional<std::vector<uint8_t>> data = detail::ReadBinFile(std::string(path));
			if (!data.has_value()) {
				return std::nullopt;
			}
			return Deserialize(data.value(), PinInShared, index);
		}
		std::vector<uint8_t> Serialize()const;
		//返回真代表写入成功
		bool SerializeToFile(std::string_view path)const {
			return detail::WriteBinFile(std::string(path), Serialize());
		}

		constexpr Logic GetLogic()const noexcept {
			return logic;
		}
		size_t put(std::string_view keyword);//插入待搜索项，内部无查重，大小写敏感 返回的size_t为内部的id，可以通过GetStrById重新获得对应字符串，可以利用这个建立映射关系
		//不要传入空字符串执行搜索，这是最坏情况，最浪费性能！
		std::vector<std::string> ExecuteSearch(std::string_view s);//执行搜索
		std::unordered_set<size_t> ExecuteSearchGetSet(std::string_view s);//执行搜索，但是返回的是内部的结果集id
		std::string GetStrById(size_t id) {//配套使用。id请使用ExecuteSearchGetSet返回的合法的来源
			return strs.getstr(id);
		}

		/* For CAPI START*/
		/* 暴露给CAPI用的，如果你是用C++开发，你不应该考虑这些公开接口 */
		//获取字符串大小，不包含终止符
		size_t GetStrSizeById(size_t id) {
			return strs.getStrSize(id);
		}
		//根据提供的缓冲区写入字符串数据，如果数据因为缓冲区大小被截断了，那么返回的是-1。完整的插入了则是0
		int PutToCharBufById(size_t id, char* buf, size_t bufSize) {
			return strs.PutToCharBuf(id, buf, bufSize);
		}
		/* For CAPI END */

		//单位是四字节
		void StrPoolReserve(size_t Newcapacity) {
			strs.reserve(Newcapacity);
		}

		void refresh() {//手动尝试刷新
			ticket->renew();
		}
		PinIn& GetPinIn() noexcept {
			return *context;
		}
		const PinIn& GetPinIn()const noexcept {
			return *context;
		}
		std::shared_ptr<PinIn> GetPinInShared() noexcept {//返回这个对象的智能指针，让你可以共享到其他TreeSearcher
			return context;
		}
		[[deprecated("It is no longer of use")]] void ClearFreeList() {
			/*NDensePool.ClearFreeList();
			NSlicePool.ClearFreeList();
			NMapPool.ClearFreeList();*/
		}
		void ShrinkToFit() {//调用的是std::vector<char>::shrink_to_fit
			strs.ShrinkToFit();
			acc.ShrinkToFit();
		}
	private:
		void init() {
			root = detail::SlabUniqueObj<Node>(NDensePool.NewObj());
			acc.setProvider(&strs);
			ticket = context->ticket([this]() {
				for (const auto& i : this->naccs) {
					i->reload(*this);
				}
				this->acc.reset();
			});
		}
		void CommonSearch(std::string_view s, std::unordered_set<size_t>& ret) {
			ticket->renew();
			acc.search(s);
			root->get(*this, ret, 0);
		}
		enum class NodeType : uint8_t {
			NDenseType, NSliceType, NMapType, NAccType
		};

		class Node {//节点类本身是私有的就行了，构造函数公有但外部不需要知道存在节点类
		public://节点类中用参数传递TreeSearcher的引用比类成员要高效，因为类成员要走this指针解析，第一个参数传引用在x64环境下一般是寄存器传递，绕过了this指针中间商，所以构建速度变更快了
			virtual ~Node() = default;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& result, size_t offset) = 0;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& result) = 0;
			//为了实现节点替换行为，我已经在API内约定好了，返回一个它本身或者一个新的Node指针，所以前后不一致的时候重设，并且new的方法不会持有这个指针
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id) = 0;
			virtual Node* putRange(TreeSearcher& p, size_t start, size_t end) = 0;
			//要求递归的去序列化节点数据
			virtual void Serialize(std::vector<uint8_t>& data)const = 0;

			//将自身载入对象池
			virtual NodeType GetNodeType()const = 0;
			static detail::SlabUniqueObj<Node> Deserialize(TreeSearcher& p, detail::VecU8Reader& reader);
		};
		//将Node*的所有权通过FreeToPool转移到对象池中，随后放弃其所有权并重设为新指针
		void NodeOwnershipReset(detail::SlabUniqueObj<Node>& smartPtrObj, Node* newPtr) {
			switch (smartPtrObj->GetNodeType()) {
			case NodeType::NDenseType: {
				NDensePool.FreeObj(static_cast<NDense*>(smartPtrObj.release()));
				break;
			}
			case NodeType::NSliceType: {
				NSlicePool.FreeObj(static_cast<NSlice*>(smartPtrObj.release()));
				break;
			}
			case NodeType::NMapType: {
				NMapPool.FreeObj(static_cast<NMap*>(smartPtrObj.release()));
				break;
			}
			case NodeType::NAccType: {
				NAccPool.FreeObj(static_cast<NAcc*>(smartPtrObj.release()));
				break;
			}
			}
			smartPtrObj.reset(newPtr);
		}

		class NDense : public Node {//密集节点本质上就是数组
		public:
			virtual ~NDense() = default;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset);
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret);
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
			virtual Node* putRange(TreeSearcher& p, size_t start, size_t end);
			virtual void Serialize(std::vector<uint8_t>& data)const;
			static detail::SlabUniqueObj<NDense> Deserialize(TreeSearcher& p, detail::VecU8Reader& reader);
			virtual NodeType GetNodeType()const {
				return NodeType::NDenseType;
			}
		private:
			size_t match(const TreeSearcher& p)const;//寻找最长公共前缀 长度
			detail::SVOArray<size_t, 2> data;
		};

		class NSlice : public Node {//切片节点，有公共前缀，用NMap/NAcc管理
		public:
			virtual ~NSlice() = default;
			NSlice(TreeSearcher& p, size_t start, size_t end) :start{ start }, end{ end } {
				exit_node = detail::SlabUniqueObj<Node>(p.NMapPool.NewObj());
			}
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset) {
				get(p, ret, offset, 0);
			}
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret) {
				exit_node->get(p, ret);
			}
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
			virtual Node* putRange(TreeSearcher& p, size_t start, size_t end);
			virtual void Serialize(std::vector<uint8_t>& data)const;
			static detail::SlabUniqueObj<NSlice> Deserialize(TreeSearcher& p, detail::VecU8Reader& reader);
			virtual NodeType GetNodeType()const {
				return NodeType::NSliceType;
			}
			NSlice(size_t start, size_t end) :start{ start }, end{ end } {}
		private:
			void cut(TreeSearcher& p, size_t offset);
			void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset, size_t start);
			detail::SlabUniqueObj<Node> exit_node = nullptr;
			size_t start;
			size_t end;
		};

		class NAcc;
		template<bool CanUpgrade>//类策略模式，运行时比较开销放到编译时
		class NMapTemplate : public Node {//分支节点，有可升级版本和不可升级版本
		public:
			virtual ~NMapTemplate() = default;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset);
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret);
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
			virtual Node* putRange(TreeSearcher& p, size_t start, size_t end);

			virtual void Serialize(std::vector<uint8_t>& data)const;
			static detail::SlabUniqueObj<NMapTemplate> Deserialize(TreeSearcher& p, detail::VecU8Reader& reader);

			virtual NodeType GetNodeType()const {
				return NodeType::NMapType;
			}
		private:
			friend NSlice;//分支时需要调用私有接口进行指针操作
			friend NAcc;//需要从可升级的节点中直接窃取成员
			void init() {//如果是不可升级的版本，则是一个无用的init函数
				if constexpr (CanUpgrade) {
					if (children == nullptr) {
						children = std::make_unique<detail::AdaptiveMap<uint32_t, detail::SlabUniqueObj<Node>>>();
					}
				}
			}
			void putNode(const uint32_t ch, detail::SlabUniqueObj<Node> n) {
				if constexpr (CanUpgrade) {//可升级模式需要懒加载代码，不可升级模式会有构造方移动原始数据，始终安全
					init();
				}
				children->insert_or_assign(ch, std::move(n));
			}
			std::unique_ptr<detail::AdaptiveMap<uint32_t, detail::SlabUniqueObj<Node>>> children = nullptr;
			detail::AdaptiveSet<size_t, true> leaves;//经常出现占用较少情况，适合做升级优化
		};
		using NMap = NMapTemplate<true>;//会自动升级的版本
		using NMapOwned = NMapTemplate<false>;//不会自动升级的版本，给NAcc类用的，升级过程中自动窃取了其成员，所以用了模板元编程技术去掉懒加载模式

		class NAcc : public Node {//加速节点 组合而非继承，不会升级的节点
		public:
			virtual ~NAcc() = default;
			NAcc(TreeSearcher& p, NMap& src) {
				GetOwned(src);//获取所有权，本质上相当于原始代码里的那个引用拷贝
				reload(p);
				p.naccs.push_back(this);
			}
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& result, size_t offset);
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& result) {
				NodeMap.get(p, result);//直接调用原始的版本，因为原版Java代码写的是继承，所以没有显式实现
			}
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
			virtual Node* putRange(TreeSearcher& p, size_t start, size_t end);

			void reload(TreeSearcher& p);
			virtual void Serialize(std::vector<uint8_t>& data)const;
			static detail::SlabUniqueObj<NAcc> Deserialize(TreeSearcher& p, detail::VecU8Reader& reader);
			virtual NodeType GetNodeType()const {
				return NodeType::NAccType;
			}
		private:
			void GetOwned(NMap& src) {
				NodeMap.children = std::move(src.children);
				NodeMap.leaves = std::move(src.leaves);
			}
			void indexUseCache(TreeSearcher& p, const uint32_t c);
			void indexNotUseCache(TreeSearcher& p, const uint32_t c);
			std::unordered_map<PinIn::Phoneme*, detail::AdaptiveSet<uint32_t>> index_node;
			NMapOwned NodeMap;
		};

		detail::SlabAllocator<NDense, 2048> NDensePool;
		detail::SlabAllocator<NSlice, 128> NSlicePool;
		detail::SlabAllocator<NMap, 512> NMapPool;
		detail::SlabAllocator<NAcc, 512> NAccPool;

		//密集节点转换临界点 原始版本是128，因为还用一个元素代表了存储的元素列表，这里直接把字符串本身当作元素
		//但是因为字符串id本身也需要记录，所以还是128
		constexpr static size_t NDenseThreshold = 128;
		//表节点转换临界点
		constexpr static size_t NMapThreshold = 32;
		const Logic logic;
		std::shared_ptr<PinIn> context = nullptr;//PinIn
		detail::Accelerator acc;

		std::unique_ptr<PinIn::Ticket> ticket;
		detail::UTF8StringPool strs;

		detail::SlabUniqueObj<Node> root = nullptr;
		std::deque<NAcc*> naccs;//观察者，不持有数据
	};

	/* NMapTemplate 实现(避免前面的声明过长) */
	template<bool CanUpgrade>
	void TreeSearcher::NMapTemplate<CanUpgrade>::get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset) {
		if (p.acc.search().size() == offset) {
			if (p.logic == Logic::EQUAL) {
				leaves.AddToSTLSet(ret);
			}
			else {
				get(p, ret);
			}
		}
		else {
			if constexpr (CanUpgrade) {
				if (children == nullptr) {//可升级模式需要判断children的有效性，但是不可升级模式下本身是由children过大而引起的升级，所以不需要判断有效性
					return;
				}
			}
			children->forEach([&](const auto& c, const auto& n) {
				detail::IndexSet::IndexSetIterObj it = p.acc.get(c, offset).GetIterObj();
				for (uint32_t i = it.Next(); i != it.end(); i = it.Next()) {
					n->get(p, ret, offset + i);
				}
			});
		}
	}

	template<bool CanUpgrade>
	void TreeSearcher::NMapTemplate<CanUpgrade>::get(TreeSearcher& p, std::unordered_set<size_t>& ret) {
		leaves.AddToSTLSet(ret);
		if constexpr (CanUpgrade) {//可升级模式需要判断children的有效性，但是不可升级模式下本身是由children过大而引起的升级，所以不需要判断有效性
			if (children == nullptr) {
				return;
			}
		}
		children->forEach([&](const auto&, const auto& v) {
			v->get(p, ret);
		});
	}

	template<bool CanUpgrade>//避免循环依赖，模板实现滞后
	TreeSearcher::Node* TreeSearcher::NMapTemplate<CanUpgrade>::put(TreeSearcher& p, size_t keyword, size_t id) {
		if (p.strs.end(keyword)) {//字符串视图不会尝试指向一个\0的字符，用end判断是最安全且合法的
			leaves.insert(id);
		}
		else {
			if constexpr (CanUpgrade) {//可升级模式需要懒加载代码，不可升级模式会有构造方移动原始数据，始终安全
				init();
			}
			uint32_t ch = p.strs.getcharFourCC(keyword);
			auto it = children->find(ch);//查找
			if (it == nullptr) {
				detail::SlabUniqueObj<NDense> ndense = detail::SlabUniqueObj<NDense>(p.NDensePool.NewObj());
				Node* result = ndense->put(p, keyword + 1, id);//无虚函数调用开销
				detail::SlabUniqueObj<Node> NewPtr = detail::SlabUniqueObj<Node>(ndense.release());
				if (result != NewPtr.get()) {
					p.NodeOwnershipReset(NewPtr, result);
				}
				children->insert_or_assign(ch, std::move(NewPtr));
			}
			else {
				detail::SlabUniqueObj<Node>& SmartSrc = *it;
				Node* result = SmartSrc->put(p, keyword + 1, id);
				if (result != SmartSrc.get()) {
					p.NodeOwnershipReset(SmartSrc, result);
				}
			}
		}
		if constexpr (CanUpgrade) {
			if (children != nullptr && children->size() > NMapThreshold) {
				return p.NAccPool.NewObj(p, *this);
			}
			return this;
		}
		else {//编译时分支
			return this;
		}
	}

	template<bool CanUpgrade>
	void TreeSearcher::NMapTemplate<CanUpgrade>::Serialize(std::vector<uint8_t>& data)const {
		if constexpr (CanUpgrade) {
			data.push_back(static_cast<uint8_t>(NodeType::NMapType));
		}
		detail::PushQWUint8(data, leaves.size());
		leaves.forEach([&](size_t i) {
			detail::PushQWUint8(data, i);
		});
		if constexpr (CanUpgrade) {
			if (children == nullptr) {//没指针插入代表假的数字
				detail::PushQWUint8(data, 0);
				return;
			}
		}
		detail::PushQWUint8(data, children->size());
		children->forEach([&](const auto& k, const auto& v) {
			detail::PushDWUint8(data, k);
			v->Serialize(data);
		});
	}

	template<bool CanUpgrade>
	detail::SlabUniqueObj<TreeSearcher::NMapTemplate<CanUpgrade>> TreeSearcher::NMapTemplate<CanUpgrade>::Deserialize(TreeSearcher& p, detail::VecU8Reader& reader) {
		size_t leavesSize = reader.GetSizeTFromQW();
		detail::SlabUniqueObj<NMapTemplate> result = detail::SlabUniqueObj<NMapTemplate>(p.NMapPool.NewObj());

		result->leaves = detail::AdaptiveSet<size_t, true>(leavesSize);
		for (size_t i = 0; i < leavesSize; i++) {
			result->leaves.DeserializationInsert(reader.GetSizeTFromQW());
		}
		size_t childrenSize = reader.GetSizeTFromQW();
		if (childrenSize == 0) {
			return result;
		}
		result->children = std::make_unique<detail::AdaptiveMap<uint32_t, detail::SlabUniqueObj<Node>>>();
		for (size_t i = 0; i < childrenSize; i++) {
			uint32_t fourCC = reader.GetDoubleWord();
			result->children->insert_or_assign(fourCC, Node::Deserialize(p, reader));
		}
		return result;
	}

	template<bool CanUpgrade>//避免循环依赖，模板实现滞后
	TreeSearcher::Node* TreeSearcher::NMapTemplate<CanUpgrade>::putRange(TreeSearcher& p, size_t start, size_t end) {
		if constexpr (CanUpgrade) {//可升级模式需要懒加载代码，不可升级模式会有构造方移动原始数据，始终安全
			init();
		}
		/*
		putRange本质上就是在模仿根据范围遍历。put方法里的
		if (p.strs.end(keyword)) {//字符串视图不会尝试指向一个\0的字符，用end判断是最安全且合法的
			leaves.insert(id);
		}
		这段代码，不可能发生在根据指定范围遍历的情况，因为不包括终止字符长度
		这个本意是当整个字符串字典树构造到末尾的时候，由map自己承担这个节点，因为不需要再构建额外的树了
		因为指定范围的遍历并不会包括终止字符，所以不需要
		*/
		for (size_t i = start; i < end; i++) {
			uint32_t ch = p.strs.getcharFourCC(i);
			auto it = children->find(ch);//查找
			if (it == nullptr) {
				detail::SlabUniqueObj<NDense> ndense = detail::SlabUniqueObj<NDense>(p.NDensePool.NewObj());
				Node* result = ndense->put(p, i + 1, start);//无虚函数调用开销
				detail::SlabUniqueObj<Node> NewPtr = detail::SlabUniqueObj<Node>(ndense.release());
				if (result != NewPtr.get()) {
					p.NodeOwnershipReset(NewPtr, result);
				}
				children->insert_or_assign(ch, std::move(NewPtr));
			}
			else {
				detail::SlabUniqueObj<Node>& SmartSrc = *it;
				Node* result = SmartSrc->put(p, i + 1, start);
				if (result != SmartSrc.get()) {
					p.NodeOwnershipReset(SmartSrc, result);
				}
			}
		}
		if constexpr (CanUpgrade) {
			if (children != nullptr && children->size() > NMapThreshold) {
				return p.NAccPool.NewObj(p, *this);
			}
			return this;
		}
		else {//编译时分支
			return this;
		}
	}
}
