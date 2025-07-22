#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <unordered_map>
#include <iostream>

#include "PinIn.h"
#include "StringPool.h"
#include "Accelerator.h"
#include "Keyboard.h"

namespace PinInCpp {
	enum class Logic : uint8_t {//不需要很多状态的枚举类
		BEGIN, CONTAIN, EQUAL
	};

	class TreeSearcher {
	public:
		virtual ~TreeSearcher() = default;
		TreeSearcher(Logic logic, const std::string_view& PinyinDictionaryPath)
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
		//因为绑定着this指针，所以不能移动和拷贝
		TreeSearcher(const TreeSearcher&) = delete;
		TreeSearcher(TreeSearcher&&) = delete;
		TreeSearcher& operator=(TreeSearcher&& src) = delete;

		void put(const std::string_view& keyword);//插入待搜索项，内部无查重，大小写敏感
		//不要传入空字符串执行搜索，这是最坏情况，最浪费性能！
		std::vector<std::string> ExecuteSearch(const std::string_view& s);//执行搜索
		std::vector<std::string_view> ExecuteSearchView(const std::string_view& s);//执行搜索，但是返回的字符串为只读视图，注意，这些视图可能会在插入新数据后变成悬垂视图！
		std::unordered_set<size_t> ExecuteSearchGetSet(const std::string_view& s);//执行搜索，但是返回的是内部的结果集id
		std::string GetStrById(size_t id) {//配套使用。id请使用ExecuteSearchGetSet返回的合法的来源
			return strs.getstr(id);
		}
		std::string_view GetStrViewById(size_t id)const {//注意，这些视图可能会在插入新数据后变成悬垂视图！
			return strs.getstr_view(id);
		}
		//单位是字节
		void StrPoolReserve(size_t _Newcapacity) {
			strs.reserve(_Newcapacity);
		}

		void refresh() {//手动尝试刷新
			ticket->renew();
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
		void init() {
			root = std::make_unique<NDense>();
			acc.setProvider(&strs);
			ticket = context->ticket([this]() {
				for (const auto& i : this->naccs) {
					i->reload(*this);
				}
				this->acc.reset();
			});
		}
		void CommonSearch(const std::string_view& s, std::unordered_set<size_t>& ret) {
			ticket->renew();
			acc.search(s);
			root->get(*this, ret, 0);
		}
		template<typename value>
		class ObjSet {//这是专门用于优化的类，本身功能并不多！
		private:
			class AbstractSet {//集合不承担查找，这个就很简单
			public:
				virtual ~AbstractSet() = default;
				virtual AbstractSet* insert(const value& input_v) = 0;
				virtual void AddToSTLSet(std::unordered_set<value>& input_v) = 0;//有点反客为主了
			};
			class HashSet : public AbstractSet {
			public:
				virtual AbstractSet* insert(const value& input_v) {
					data.insert(input_v);
					return this;
				}
				virtual void AddToSTLSet(std::unordered_set<value>& input_v) {
					for (const value& v : data) {
						input_v.insert(v);
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
					data.push_back(input_v);
					if (data.size() > ContainerThreshold) {
						std::unique_ptr<HashSet> result = std::make_unique<HashSet>();
						for (const value& v : data) {
							result->insert(v);
						}
						return result.release();
					}
					return this;
				}
				virtual void AddToSTLSet(std::unordered_set<value>& input_v) {
					for (const value& v : data) {
						input_v.insert(v);
					}
				}
			private:
				std::vector<value> data;
			};
			std::unique_ptr<AbstractSet> Container;
		public:
			ObjSet() :Container{ std::make_unique<ArraySet>() } {}
			ObjSet(std::initializer_list<value> list) :Container{ std::make_unique<ArraySet>() } {
				for (const value& v : list) {
					insert(v);
				}
			}
			void insert(const value& input_v) {
				AbstractSet* set = Container->insert(input_v);
				if (set != Container.get()) {
					Container.reset(set);
				}
			}
			void AddToSTLSet(std::unordered_set<value>& input_v) {
				Container->AddToSTLSet(input_v);
			}
		};
		class Node {//节点类本身是私有的就行了，构造函数公有但外部不需要知道存在节点类
		public://节点类中用参数传递TreeSearcher的引用比类成员要高效，因为类成员要走this指针解析，第一个参数传引用在x64环境下一般是寄存器传递，绕过了this指针中间商，所以构建速度变更快了
			virtual ~Node() = default;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& result, size_t offset) = 0;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& result) = 0;
			//为了实现节点替换行为，我已经在API内约定好了，返回一个它本身或者一个新的Node指针，所以前后不一致的时候重设，并且new的方法不会持有这个指针
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id) = 0;
		};
		class NDense : public Node {//密集节点本质上就是数组
		public:
			virtual ~NDense() = default;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset);
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret);
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
		private:
			friend TreeSearcher;
			size_t match(const TreeSearcher& p)const;//寻找最长公共前缀 长度
			std::vector<size_t> data;
		};
		class NSlice;
		class NAcc;

		template<bool CanUpgrade>//类策略模式，运行时比较开销放到编译时
		class NMapTemplate : public Node {
		public:
			virtual ~NMapTemplate() = default;
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset);
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret) {
				leaves.AddToSTLSet(ret);
				if constexpr (CanUpgrade) {//可升级模式需要判断children的有效性，但是不可升级模式下本身是由children过大而引起的升级，所以不需要判断有效性
					if (children != nullptr) {
						for (const auto& v : *children) {
							v.second->get(p, ret);
						}
					}
				}
				else {
					for (const auto& v : *children) {
						v.second->get(p, ret);
					}
				}
			}
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
		private:
			friend NSlice;
			friend NAcc;
			void init() {//如果是不可升级的版本，则是一个无用的init函数
				if constexpr (CanUpgrade) {
					if (children == nullptr) {
						children = std::make_unique<std::unordered_map<uint32_t, std::unique_ptr<Node>>>();
					}
				}
			}
			Node* put(const uint32_t ch, std::unique_ptr<Node> n) {
				if constexpr (CanUpgrade) {//可升级模式需要懒加载代码，不可升级模式会有构造方移动原始数据，始终安全
					init();
				}
				//它本质上是什么？所有权转移，那么前后指针，实际上是不变的
				Node* result = n.get();
				children->insert_or_assign(ch, std::move(n));
				return result;
			}
			void reset_children(const uint32_t ch, Node* n) {
				children->operator[](ch).reset(n);
			}
			std::unique_ptr<std::unordered_map<uint32_t, std::unique_ptr<Node>>> children = nullptr;
			ObjSet<size_t> leaves;//经常出现占用较少情况，适合做升级优化
		};
		using NMap = NMapTemplate<true>;//会自动升级的版本
		using NMapOwned = NMapTemplate<false>;//不会自动升级的版本，给NAcc类用的，升级过程中自动窃取了其成员，所以用了模板元编程技术去掉懒加载模式

		class NAcc : public Node {//组合而非继承
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
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id) {
				NodeMap.put(p, keyword, id);//绝对不会升级，不需要检查
				index(p, p.strs.getcharFourCC(keyword));//put完后构建索引，并且不再有put操作，应该是安全的
				return this;
			}
			void reload(TreeSearcher& p) {
				index_node.clear();//释放所有音素
				for (const auto& [k, v] : *NodeMap.children) {
					index(p, k);
				}
			}
		private:
			void GetOwned(NMap& src) {
				NodeMap.children = std::move(src.children);
				NodeMap.leaves = std::move(src.leaves);
			}
			void index(TreeSearcher& p, const uint32_t c);
			//这个就不做升级优化了，通常都很多，做升级优化内存降下来不明显还引入了更多的运行时开销，有明显的性能下降
			std::unordered_map<PinIn::Phoneme, std::unordered_set<uint32_t>> index_node;
			NMapOwned NodeMap;
		};

		class NSlice : public Node {
		public:
			virtual ~NSlice() = default;
			NSlice(size_t start, size_t end) :start{ start }, end{ end } {
				exit_node = std::make_unique<NMap>();
			}
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset) {
				get(p, ret, offset, 0);
			}
			virtual void get(TreeSearcher& p, std::unordered_set<size_t>& ret) {
				exit_node->get(p, ret);
			}
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
		private:
			void cut(TreeSearcher& p, size_t offset);
			void get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset, size_t start);
			std::unique_ptr<Node> exit_node = nullptr;
			size_t start;
			size_t end;
		};

		//密集节点转换临界点 原始版本是128，因为还用一个元素代表了存储的元素列表，这里直接把字符串本身当作元素
		//但是因为字符串id本身也需要记录，所以还是128
		constexpr static int NDenseThreshold = 128;
		//表节点转换临界点
		constexpr static int NMapThreshold = 32;
		//ObjSet转换临界点
		constexpr static int ContainerThreshold = 128;
		std::shared_ptr<PinIn> context = nullptr;//PinIn
		std::unique_ptr<PinIn::Ticket> ticket;
		UTF8StringPool strs;//应当继续贯彻零拷贝设计
		Accelerator acc;
		Logic logic;

		std::unique_ptr<Node> root = nullptr;
		std::vector<NAcc*> naccs;//观察者，不持有数据
	};

	/* 过长的模板实现 */
	template<bool CanUpgrade>//避免循环依赖，模板实现滞后
	void TreeSearcher::NMapTemplate<CanUpgrade>::get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset) {
		if constexpr (CanUpgrade) {//可升级模式需要判断children的有效性，但是不可升级模式下本身是由children过大而引起的升级，所以不需要判断有效性
			if (p.acc.search().size() == offset) {
				if (p.logic == Logic::EQUAL) {
					leaves.AddToSTLSet(ret);
				}
				else {
					get(p, ret);
				};
			}
			else if (children != nullptr) {
				for (const auto& [c, n] : *children) {
					IndexSet::IndexSetIterObj it = p.acc.get(c, offset).GetIterObj();
					for (uint32_t i = it.Next(); i != IndexSetIterEnd; i = it.Next()) {
						n->get(p, ret, offset + i);
					}
				}
			}
		}
		else {
			if (p.acc.search().size() == offset) {
				if (p.logic == Logic::EQUAL) {
					leaves.AddToSTLSet(ret);
				}
				else {
					get(p, ret);
				};
			}
			else {
				for (const auto& [c, n] : *children) {
					IndexSet::IndexSetIterObj it = p.acc.get(c, offset).GetIterObj();
					for (uint32_t i = it.Next(); i != IndexSetIterEnd; i = it.Next()) {
						n->get(p, ret, offset + i);
					}
				}
			}
		}
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
			Node* sub;
			if (it == children->end()) {
				sub = put(ch, std::make_unique<NDense>());
			}
			else {
				sub = it->second.get();
			}
			Node* src = sub;
			sub = sub->put(p, keyword + 1, id);
			if (src != sub) {
				reset_children(ch, sub);
			}
		}
		if constexpr (CanUpgrade) {
			if (children != nullptr && children->size() > NMapThreshold) {
				return new NAcc(p, *this);
			}
			return this;
		}
		else {//编译时分支
			return this;
		}
	}
}
