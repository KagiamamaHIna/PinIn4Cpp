#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <unordered_map>

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
		TreeSearcher(Logic logic, const std::string& PinyinDictionary)
			:logic{ logic }, context(PinyinDictionary), acc(context) {
			root = std::make_unique<NDense>(*this);
			acc.setProvider(strs.get());
			ticket = context.ticket([this]() {
				for (const auto& i : this->naccs) {
					i->reload();
				}
				this->acc.reset();
			});
		}

		void put(const std::string& keyword);
		std::vector<std::string> ExecuteSearch(const std::string& s);
		std::vector<std::string_view> ExecuteSearchView(const std::string& s);
		void refresh() {
			ticket->renew();
		}
		const PinIn& GetPinIn()const {
			return context;
		}
		PinIn& GetPinIn() {
			return context;
		}
	private://节点类本身是私有的就行了，构造函数公有但外部不需要知道存在节点类
		class Node {
		public:
			virtual ~Node() = default;
			virtual void get(std::unordered_set<size_t>& result, size_t offset) = 0;
			virtual void get(std::unordered_set<size_t>& result) = 0;
			//为了实现节点替换行为，我已经在API内约定好了，返回一个它本身或者一个新的Node指针，所以前后不一致的时候重设，并且new的方法不会持有这个指针
			virtual Node* put(size_t keyword, size_t id) = 0;
		protected:
			Node(TreeSearcher& p) :p{ p } {}
			TreeSearcher& p;
		};
		class NDense : public Node {//密集节点本质上就是数组
		public:
			virtual ~NDense() = default;
			NDense(TreeSearcher& p) : Node(p) {}
			virtual void get(std::unordered_set<size_t>& ret, size_t offset);
			virtual void get(std::unordered_set<size_t>& ret);
			virtual Node* put(size_t keyword, size_t id);
		private:
			friend TreeSearcher;
			size_t match();//寻找最长公共前缀 长度
			std::vector<size_t> data;
		};
		class NSlice;
		class NAcc;

		template<bool CanUpgrade>//类策略模式，运行时比较开销放到编译时
		class NMapTemplate : public Node {
		public:
			NMapTemplate(TreeSearcher& p) : Node(p) {}
			virtual ~NMapTemplate() = default;
			virtual void get(std::unordered_set<size_t>& ret, size_t offset) {
				if (p.acc.search().size() == offset) {
					if (p.logic == Logic::EQUAL) {
						for (const auto& v : leaves) {
							ret.insert(v);
						}
					}
					else {
						get(ret);
					};
				}
				else if (children != nullptr) {
					for (const auto& [c, n] : *children.get()) {
						p.acc.get(c, offset).foreach([&](uint32_t i) {
							n->get(ret, offset + i);
						});
					}
				}
			}
			virtual void get(std::unordered_set<size_t>& ret) {
				for (const auto& v : leaves) {
					ret.insert(v);
				}
				if (children != nullptr) {
					for (const auto& [c, n] : *children.get()) {
						n->get(ret);
					}
				}
			}
			virtual Node* put(size_t keyword, size_t id);
		private:
			friend NSlice;
			friend NAcc;
			void init() {
				if (children == nullptr) {
					children = std::make_unique<std::unordered_map<std::string, std::unique_ptr<Node>>>();
				};
			}
			Node* put(const std::string& ch, std::unique_ptr<Node> n) {
				init();
				//它本质上是什么？所有权转移，那么前后指针，实际上是不变的
				Node* result = n.get();
				children->insert_or_assign(ch, std::move(n));
				return result;
			}
			void reset_children(const std::string& ch, Node* n) {
				children->operator[](ch).reset(n);
			}
			std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<Node>>> children = nullptr;
			std::unordered_set<size_t> leaves;
		};
		using NMap = NMapTemplate<true>;//会自动升级的版本
		using NMapOwned = NMapTemplate<false>;//不会自动升级的版本，给NAcc类用的

		class NAcc : public Node {//组合而非继承
		public:
			virtual ~NAcc() = default;
			NAcc(TreeSearcher& p, NMap& src) :Node(p), NodeMap{ p } {
				GetOwned(src);//获取所有权，本质上相当于原始代码里的那个引用拷贝
				reload();
				p.naccs.push_back(this);
			}
			virtual void get(std::unordered_set<size_t>& result, size_t offset);
			virtual void get(std::unordered_set<size_t>& result) {
				NodeMap.get(result);//直接调用原始的版本，因为原版Java代码写的是继承，所以没有显式实现
			}
			virtual Node* put(size_t keyword, size_t id) {
				NodeMap.put(keyword, id);//绝对不会升级，不需要检查
				index(p.strs->getchar(keyword));
				return this;
			}
			void reload() {
				index_node.clear();
				for (const auto& [k, v] : *(NodeMap.children.get())) {
					index(k);
				}
			}
		private:
			void GetOwned(NMap& src) {
				NodeMap.children = std::move(src.children);
				NodeMap.leaves = std::move(src.leaves);
			}
			void index(const std::string& c);
			std::unordered_map<PinIn::Phoneme, std::unordered_set<std::string>> index_node;
			NMapOwned NodeMap;
		};

		class NSlice : public Node {
		public:
			virtual ~NSlice() = default;
			NSlice(size_t start, size_t end, TreeSearcher& p) :start{ start }, end{ end }, Node(p) {
				exit_node = std::make_unique<NMap>(p);
			}
			virtual void get(std::unordered_set<size_t>& ret, size_t offset) {
				get(ret, offset, 0);
			}
			virtual void get(std::unordered_set<size_t>& ret) {
				exit_node->get(ret);
			}
			virtual Node* put(size_t keyword, size_t id);
		private:
			void cut(size_t offset);
			void get(std::unordered_set<size_t>& ret, size_t offset, size_t start);
			std::unique_ptr<Node> exit_node = nullptr;
			size_t start;
			size_t end;
		};

		//密集节点转换临界点 原始版本是128，因为还用一个元素代表了存储的元素列表，这里直接把字符串本身当作元素
		//但是因为字符串id本身也需要记录，所以还是128
		constexpr static int NDenseThreshold = 128;
		constexpr static int NAccThreshold = 32;
		std::unique_ptr<StringPoolBase> strs = std::make_unique<UTF8StringPool>();;//应当继续贯彻零拷贝设计
		Logic logic;
		PinIn context;//PinIn
		std::unique_ptr<PinIn::Ticket> ticket;
		Accelerator acc;

		std::unique_ptr<Node> root = nullptr;
		std::vector<NAcc*> naccs;//观察者，不持有数据
	};

	template<bool CanUpgrade>//避免循环依赖，模板实现滞后
	TreeSearcher::Node* TreeSearcher::NMapTemplate<CanUpgrade>::put(size_t keyword, size_t id) {
		if (p.strs->end(keyword)) {//字符串视图不会尝试指向一个\0的字符，用empty判断是最安全且合法的
			leaves.insert(id);
		}
		else {
			init();
			std::string ch = p.strs->getchar(keyword);
			auto it = children->find(ch);//查找
			Node* sub;
			if (it == children->end()) {
				sub = put(ch, std::make_unique<NDense>(p));
			}
			else {
				sub = it->second.get();
			}
			Node* src = sub;
			sub = sub->put(keyword + 1, id);
			if (src != sub) {
				reset_children(ch, sub);
			}
		}
		if constexpr (CanUpgrade) {
			if (children != nullptr && children->size() > NAccThreshold) {
				return new NAcc(p, *this);
			}
			return this;
		}
		else {//编译时分支
			return this;
		}
	}
}
