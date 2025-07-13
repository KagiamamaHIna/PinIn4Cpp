#pragma once

#include <string>
#include <vector>
#include <set>
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
		TreeSearcher(Logic logic, const PinIn& input_context, std::unique_ptr<StringPoolBase> strpool = nullptr)//你不应该持有strpool!
			:logic{ logic }, context{ input_context }, acc{ Accelerator(strs, context) } {
			if (strpool == nullptr) {//如果没有指针，就分配一个UTF8的进去
				strs = std::make_unique<UTF8StringPool>();
			}
			else {
				strs = std::move(strpool);//显式移动，代表所有权转移
			}
		}

		void put(const std::string& keyword);
	private:
		class Node {
		public:
			virtual ~Node() = default;
			virtual void get(TreeSearcher& p, std::set<size_t>& result, size_t offset) = 0;
			virtual void get(TreeSearcher& p, std::set<size_t>& result) = 0;
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id) = 0;
		};

		class NMap : public Node {
		public:
			virtual void get(TreeSearcher& p, std::set<size_t>& ret, size_t offset) {

			}
			virtual void get(TreeSearcher& p, std::set<size_t>& ret) {
			}
			virtual Node& put(TreeSearcher& p, size_t name) {
				return *this;
			}
		private:
			std::unordered_map<std::string, std::unique_ptr<Node>> children;
			std::set<size_t> leaves;
		};

		class NSlice : public Node {
		public:
			NSlice(size_t start, size_t end) :start{ start }, end{ end } {}
			virtual void get(TreeSearcher& p, std::set<size_t>& ret, size_t offset) {

			}
			virtual void get(TreeSearcher& p, std::set<size_t>& ret) {
			}
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id) {
				return this;
			}
		private:
			size_t start;
			size_t end;
		};

		class NDense : public Node {//密集节点本质上就是数组
		public:
			virtual void get(TreeSearcher& p, std::set<size_t>& ret, size_t offset) {

			}
			virtual void get(TreeSearcher& p, std::set<size_t>& ret) {

			}
			virtual Node* put(TreeSearcher& p, size_t keyword, size_t id);
		private:
			size_t match(TreeSearcher& p);//寻找最长公共前缀 长度
			std::vector<size_t> data;
		};

		//class NAcc : public Node {
		//public:

		//private:

		//};

		//密集节点转换临界点 原始版本是128，因为还用一个元素代表了存储的元素列表，这里直接把字符串本身当作元素
		//但是因为字符串id本身也需要记录，所以还是128
		constexpr static int NDenseThreshold = 128;
		constexpr static int NMapThreshold = 32;//分支节点转换临界点
		std::unique_ptr<StringPoolBase> strs;
		Logic logic;
		const PinIn& context;//PinIn
		Accelerator acc;

		std::unique_ptr<Node> root = std::make_unique<NDense>();
	};
}
