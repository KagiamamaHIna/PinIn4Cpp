#include "TreeSearcher.h"

namespace PinInCpp {

	void TreeSearcher::put(const std::string& keyword) {
		ticket->renew();
		size_t pos = strs.put(keyword);
		size_t end = logic == Logic::CONTAIN ? strs.getLastStrSize() : 1;
		for (size_t i = 0; i < end; i++) {
			Node* result = root->put(pos + i, pos);
			if (root.get() != result) {
				root.reset(result);
			}
		}
	}

	std::vector<std::string> TreeSearcher::ExecuteSearch(const std::string& s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);

		std::vector<std::string> result;
		result.reserve(ret.size());
		for (const auto id : ret) {//基本类型复制更高效
			result.push_back(strs.getstr(id));
		}
		return result;
	}

	std::vector<std::string_view> TreeSearcher::ExecuteSearchView(const std::string& s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);

		std::vector<std::string_view> result;
		result.reserve(ret.size());
		for (const auto id : ret) {//基本类型复制更高效
			result.push_back(strs.getstr_view(id));
		}
		return result;
	}

	std::unordered_set<size_t> TreeSearcher::ExecuteSearchGetSet(const std::string& s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);
		return ret;
	}

	void TreeSearcher::NDense::get(std::unordered_set<size_t>& ret, size_t offset) {
		bool full = p.logic == Logic::EQUAL;
		if (!full && p.acc.search().size() == offset) {
			get(ret);
		}
		else {
			for (size_t i = 0; i < data.size(); i += 2) {
				size_t ch = data[i];
				if (full ? p.acc.matches(offset, ch) : p.acc.begins(offset, ch)) {
					ret.insert(data[i + 1]);
				}
			}
		}
	}

	void TreeSearcher::NDense::get(std::unordered_set<size_t>& ret) {
		for (size_t i = 1; i < data.size(); i += 2) {
			ret.insert(data[i]);
		}
	}

	TreeSearcher::Node* TreeSearcher::NDense::put(size_t keyword, size_t id) {
		if (data.size() >= TreeSearcher::NDenseThreshold) {
			size_t pattern = data[0];
			std::unique_ptr<Node> result = std::make_unique<NSlice>(pattern, pattern + match(), p);
			Node* other = result.get();
			for (size_t j = 0; j < data.size(); j += 2) {
				other = result->put(data[j], data[j + 1]);
				if (other != result.get()) {//节点升级
					result.reset(other);
					other = result.get();
				}
			}
			other = result->put(keyword, id);
			if (other != result.get()) {//节点升级
				result.reset(other);
			}
			return result.release();
		}
		else {
			data.push_back(keyword);
			data.push_back(id);
			return this;
		}
	}

	size_t TreeSearcher::NDense::match() {//这个函数内，是不会put的，可以实现零拷贝设计
		for (size_t i = 0; ; i++) {
			if (p.strs.end(data[0] + i)) {//空检查置前，避免额外的字符串构造和std::string比较。而且end实际上比较的是字节，所以速度会更快
				return i;
			}
			std::string_view a = p.strs.getchar_view(data[0] + i);
			for (size_t j = 2; j < data.size(); j += 2) {//跳过第一个元素
				std::string_view b = p.strs.getchar_view(data[j] + i);
				if (a != b) {//空检查置前了，所以这里可以删除空检查
					return i;
				}
			}
		}
	}

	void TreeSearcher::NAcc::get(std::unordered_set<size_t>& result, size_t offset) {
		if (p.acc.search().size() == offset) {
			if (p.logic == Logic::EQUAL) {
				NodeMap.leaves.AddToSTLSet(result);
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
				if (!k.match(p.acc.search(), offset, true).empty()) {
					for (const auto& str : v) {
						p.acc.get(str, offset).foreach([&](uint32_t j) {
							this->NodeMap.children->operator[](str)->get(result, offset + j);
						});
					}
				}
			}
		}
	}

	void TreeSearcher::NAcc::index(const std::string_view& c) {
		PinIn::Character* ch = p.context->GetCharCachePtr(c);
		if (ch == nullptr) {
			PinIn::Character ch = p.context->GetChar(c);
			for (const auto& py : ch.GetPinyins()) {
				const PinIn::Phoneme& ph = py.GetPhonemes()[0];
				auto it = index_node.find(ph);
				if (it == index_node.end()) {//对应的是字符集合为空
					index_node.insert_or_assign(ph, std::unordered_set<std::string>{std::string(c)});//把汉字插进去
				}
				else {//不为空
					it->second.insert(std::string(c));
				}
			}
		}
		else {
			for (const auto& py : ch->GetPinyins()) {
				const PinIn::Phoneme& ph = py.GetPhonemes()[0];
				auto it = index_node.find(ph);
				if (it == index_node.end()) {//对应的是字符集合为空
					index_node.insert_or_assign(ph, std::unordered_set<std::string>{std::string(c)});//把汉字插进去
				}
				else {//不为空
					it->second.insert(std::string(c));
				}
			}
		}
	}

	TreeSearcher::Node* TreeSearcher::NSlice::put(size_t keyword, size_t id) {
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

	void TreeSearcher::NSlice::cut(size_t offset) {
		std::unique_ptr<NMap> insert = std::make_unique<NMap>(p);//保证异常安全
		if (offset + 1 == end) {//当前exit_node的所有权都会被转移
			insert->put(p.strs.getchar(offset), std::move(exit_node));
		}
		else {
			std::unique_ptr<NSlice> half = std::make_unique<NSlice>(offset + 1, end, p);
			half->exit_node = std::move(exit_node);
			insert->put(p.strs.getchar(offset), std::move(half));
		}
		exit_node = std::move(insert);
		end = offset;
	}

	void TreeSearcher::NSlice::get(std::unordered_set<size_t>& ret, size_t offset, size_t start) {
		if (this->start + start == end) {
			exit_node->get(ret, offset);
		}
		else if (offset == p.acc.search().size()) {
			if (p.logic != Logic::EQUAL) {
				exit_node->get(ret);
			}
		}
		else {
			std::string_view ch = p.strs.getchar_view(this->start + start);
			p.acc.get(ch, offset).foreach([&](uint32_t i) {//acc.get本身不会进行put，安全的
				get(ret, offset + i, start + 1);
			});
		}
	}
}
