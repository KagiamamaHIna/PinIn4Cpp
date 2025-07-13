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
	private:
		class Node {
		public:
			virtual ~Node() = default;
			virtual void get(std::unordered_set<size_t>& result, size_t offset) = 0;
			virtual void get(std::unordered_set<size_t>& result) = 0;
			virtual Node* put(size_t keyword, size_t id) = 0;
		protected:
			Node(TreeSearcher& p) :p{ p } {}
			TreeSearcher& p;
		};
		class NDense;
		class NSlice;
		class NMap : public Node {
		public:
			virtual void get(std::unordered_set<size_t>& ret, size_t offset) {

			}
			virtual void get(std::unordered_set<size_t>& ret) {
			}
			virtual Node* put(size_t keyword, size_t id) {
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
					if (src != sub) {//为了实现节点替换行为，我已经在API内约定好了，返回一个它本身或者一个新的Node*指针，所以前后不一致的时候重设，并且new的方法不会持有这个指针
						reset_children(ch, sub);
					}
				}
				/*return !(this instanceof NAcc) && children != null && children.size() > 32 ?
					new NAcc<>(p, this) : this;*/

				return this;//未完成，暂时这样写通过避免编辑时烦人的警告，这个注释以后也会被删除
			}
		private:
			friend NSlice;
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
			NMap(TreeSearcher& p) : Node(p) {}
			std::unique_ptr<std::unordered_map<std::string, std::unique_ptr<Node>>> children = nullptr;
			std::unordered_set<size_t> leaves;
		};

		class NSlice : public Node {
		public:
			virtual void get(std::unordered_set<size_t>& ret, size_t offset) {

			}
			virtual void get(std::unordered_set<size_t>& ret) {
			}
			virtual Node* put(size_t keyword, size_t id) {
				return this;
			}
		private:
			friend NDense;//由NDense进行节点转换的构造
			NSlice(size_t start, size_t end, TreeSearcher& p) :start{ start }, end{ end }, Node(p) {
				//exit = std::make_unique<NMap>(p);
			}
			std::unique_ptr<Node> exit = nullptr;
			size_t start;
			size_t end;
		};

		class NDense : public Node {//密集节点本质上就是数组
		public:
			virtual void get(std::unordered_set<size_t>& ret, size_t offset);
			virtual void get(std::unordered_set<size_t>& ret);
			virtual Node* put(size_t keyword, size_t id);
		private:
			friend TreeSearcher;
			NDense(TreeSearcher& p) : Node(p) {}
			size_t match();//寻找最长公共前缀 长度
			std::vector<size_t> data;
		};

		//class NAcc : public Node { 组合而非继承
		//public:

		//private:

		//};

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
	};
}
