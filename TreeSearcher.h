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
	/*设计思路：
		把待选项的字符串统一存入Compressor中
		put返回的字符串首段索引可以当作字符串唯一id
		如果需要压缩字典树，只需要标记偏移量(长度)即可
		存储额外的偏移量，当作压缩字典树

		原作者写了一个字符封装类，在上面有拼音数据
		拼音类也和我们的设计思路不一样
	*/

	enum class Logic : uint8_t {//不需要很多状态的枚举类
		BEGIN, CONTAIN, EQUAL
	};

	class TreeSearcher {
	public:
		virtual ~TreeSearcher() = default;
		TreeSearcher(Logic logic, const std::string& PinyinDictionary, std::unique_ptr<StringPoolBase> strpool = nullptr)//你不应该持有strpool!
			:logic{ logic }, context(PinyinDictionary), acc{ Accelerator(context) } {
			if (strpool == nullptr) {//如果没有指针，就分配一个UTF8的进去
				strs = std::make_unique<UTF8StringPool>();
			}
			else {
				strs = std::move(strpool);//显式移动，代表所有权转移
			}
			//root = std::make_unique<NDense>(*this);
			acc.setProvider(strs.get());
			ticket = context.ticket([]() {

			});
		}

		void put(const std::string& keyword);
	private://节点类本身是私有的就行了，构造函数公有但外部不需要知道存在节点类
		class Node {
		public:
			virtual ~Node() = default;
			virtual void get(std::unordered_set<size_t>& result, size_t offset) = 0;
			virtual void get(std::unordered_set<size_t>& result) = 0;
			//为了实现节点替换行为，我已经在API内约定好了，返回一个它本身或者一个新的Node*指针，所以前后不一致的时候重设，并且new的方法不会持有这个指针
			virtual Node* put(size_t keyword, size_t id) = 0;
		protected:
			Node(TreeSearcher& p) :p{ p } {}
			TreeSearcher& p;
		};
		class NDense : public Node {//密集节点本质上就是数组
		public:
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
			NAcc(TreeSearcher& p, NMap& src) :Node(p), NodeMap{ p } {
				GetOwned(src);//获取所有权，本质上相当于原始代码里的那个引用拷贝
				reload();
				p.naccs.push_back(this);
			}
			virtual void get(std::unordered_set<size_t>& result, size_t offset) {
				if (p.acc.search().size() == offset) {
					if (p.logic == Logic::EQUAL) {
						for (const auto& v : NodeMap.leaves) {
							result.insert(v);
						}
					}
					else {
						NodeMap.get(result);//直接调用原始的这个，避免中间层开销
					}
				}
				else {
					auto it = NodeMap.children->find(p.acc.search()[offset]);
					if (it != NodeMap.children->end()) {
						it->second->get(result, offset + 1);
					}
					for (const auto& [k, v] : index_node) {
						if (!k.match(p.acc.search().ToStream(), offset, true).empty()) {
							for (const auto& str : v) {
								p.acc.get(str, offset).foreach([&](uint32_t j) {
									this->NodeMap.children->operator[](str)->get(result, offset + j);
								});
							}
						}
					}
				}
			}
			virtual void get(std::unordered_set<size_t>& result) {
				NodeMap.get(result);//直接调用原始的版本，因为原版Java代码写的是继承，所以没有显式实现

			}
			virtual Node* put(size_t keyword, size_t id) {
				NodeMap.put(keyword, id);
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
			void index(const std::string& c) {
				PinIn::Character ch = p.context.GetChar(c);
				for (const auto& py : ch.GetPinyins()) {
					const PinIn::Phoneme& ph = py.GetPhonemes()[0];
					auto it = index_node.find(ph);
					if (it == index_node.end()) {//对应的是字符集合为空
						index_node.insert_or_assign(ph, std::unordered_set<std::string>{c});//把汉字插进去
					}
					else {//不为空
						it->second.insert(c);
					}
				}
			}
			std::unordered_map<PinIn::Phoneme, std::unordered_set<std::string>> index_node;
			NMapOwned NodeMap;
		};

		class NSlice : public Node {
		public:
			NSlice(size_t start, size_t end, TreeSearcher& p) :start{ start }, end{ end }, Node(p) {
				exit_node = std::make_unique<NMap>(p);
			}
			virtual void get(std::unordered_set<size_t>& ret, size_t offset) {
				get(ret, offset, 0);
			}
			virtual void get(std::unordered_set<size_t>& ret) {
				exit_node->get(ret);
			}
			virtual Node* put(size_t keyword, size_t id) {
				size_t length = end - start;
				size_t match = p.acc.common(start, keyword, length);
				Node* n;
				if (match >= length) {
					n = exit_node->put(keyword + length, id);
				}
				else {
					cut(start + match);
					n = exit_node->put(keyword + match, id);
				}
				if (exit_node.get() != n) {
					exit_node.reset(n);
				}
				return start == end ? exit_node.release() : this;
			}
		private:
			void cut(size_t offset) {
				NMap* insert = new NMap(p);
				if (offset + 1 == end) {//当前exit_node的所有权都会被转移
					insert->put(p.strs->getchar(offset), std::move(exit_node));
				}
				else {
					NSlice* half = new NSlice(offset + 1, end, p);
					half->exit_node = std::move(exit_node);
					insert->put(p.strs->getchar(offset), std::unique_ptr<Node>(half));
				}
				exit_node.reset(insert);
				end = offset;
			}
			void get(std::unordered_set<size_t>& ret, size_t offset, size_t start) {
				if (this->start + start == end) {
					exit_node->get(ret, offset);
				}
				else if (offset == p.acc.search().size()) {
					if (p.logic != Logic::EQUAL) {
						exit_node->get(ret);
					}
				}
				else {
					std::string ch = p.strs->getchar(this->start + start);
					p.acc.get(ch, offset).foreach([&](uint32_t i) {
						get(ret, offset + i, start + 1);
					});
				}
			}
			std::unique_ptr<Node> exit_node = nullptr;
			size_t start;
			size_t end;
		};

		//密集节点转换临界点 原始版本是128，因为还用一个元素代表了存储的元素列表，这里直接把字符串本身当作元素
		//但是因为字符串id本身也需要记录，所以还是128
		constexpr static int NDenseThreshold = 128;
		constexpr static int NMapThreshold = 32;//分支节点转换临界点
		std::unique_ptr<StringPoolBase> strs;//应当继续贯彻零拷贝设计
		Logic logic;
		const PinIn context;//PinIn
		std::unique_ptr<PinIn::Ticket> ticket;
		Accelerator acc;

		std::unique_ptr<Node> root = nullptr;
		std::vector<NAcc*> naccs;//观察者，不持有数据
	};

	template<bool CanUpgrade>//避免循环依赖，模板实现滞后
	TreeSearcher::Node* TreeSearcher::NMapTemplate<CanUpgrade>::put(size_t keyword, size_t id) {
		if (p.strs->getchar_view(keyword).empty()) {//字符串视图不会尝试指向一个\0的字符，用empty判断是最安全且合法的
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
			if (children != nullptr && children->size() > 32) {
				return new NAcc(p, *this);
			}
			return this;
		}
		else {//编译时分支
			return this;
		}
	}
}
