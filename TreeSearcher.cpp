#include "TreeSearcher.h"

namespace PinInCpp {

	void TreeSearcher::put(const std::string_view& keyword) {
		ticket->renew();
		size_t pos = strs.put(keyword);
		size_t end = logic == Logic::CONTAIN ? strs.getLastStrSize() : 1;
		for (size_t i = 0; i < end; i++) {
			Node* result = root->put(*this, pos + i, pos);
			if (root.get() != result) {
				NodeOwnershipReset(root, result);
			}
		}
	}

	std::vector<std::string> TreeSearcher::ExecuteSearch(const std::string_view& s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);

		std::vector<std::string> result;
		result.reserve(ret.size());
		for (const size_t id : ret) {//基本类型复制更高效
			result.emplace_back(strs.getstr(id));
		}
		return result;
	}

	std::vector<std::string_view> TreeSearcher::ExecuteSearchView(const std::string_view& s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);

		std::vector<std::string_view> result;
		result.reserve(ret.size());
		for (const size_t id : ret) {//基本类型复制更高效
			result.emplace_back(strs.getstr_view(id));
		}
		return result;
	}

	std::unordered_set<size_t> TreeSearcher::ExecuteSearchGetSet(const std::string_view& s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);
		return ret;
	}

	void TreeSearcher::NDense::get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset) {
		bool full = p.logic == Logic::EQUAL;
		if (!full && p.acc.search().size() == offset) {
			get(p, ret);
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

	void TreeSearcher::NDense::get(TreeSearcher& p, std::unordered_set<size_t>& ret) {
		for (size_t i = 1; i < data.size(); i += 2) {
			ret.insert(data[i]);
		}
	}

	TreeSearcher::Node* TreeSearcher::NDense::put(TreeSearcher& p, size_t keyword, size_t id) {
		if (data.size() >= TreeSearcher::NDenseThreshold) {
			size_t pattern = data[0];
			std::unique_ptr<Node> result = p.NSlicePool.NewObj(p, pattern, pattern + match(p));
			Node* other = result.get();
			for (size_t j = 0; j < data.size(); j += 2) {
				other = result->put(p, data[j], data[j + 1]);
				if (other != result.get()) {//节点升级
					p.NodeOwnershipReset(result, other);
					//因为other本质上已经是新指针了，所以不用再次赋值。result通过这个转移所有权的函数现在他和other是同一个指针
				}
			}
			other = result->put(p, keyword, id);
			if (other != result.get()) {//节点升级
				p.NodeOwnershipReset(result, other);
			}
			return result.release();
		}
		else {
			data.emplace_back(keyword);
			data.emplace_back(id);
			return this;
		}
	}

	size_t TreeSearcher::NDense::match(const TreeSearcher& p)const {//这个函数内，是不会put的，可以实现零拷贝设计
		for (size_t i = 0; ; i++) {
			if (p.strs.end(data[0] + i)) {//空检查置前，避免额外的字符串构造和std::string比较。而且end实际上比较的是字节，所以速度会更快
				return i;
			}
			size_t aIndex = data[0] + i;
			for (size_t j = 2; j < data.size(); j += 2) {//跳过第一个元素
				if (!p.strs.EqualChar(aIndex, data[j] + i)) {
					return i;
				}
			}
		}
	}

	void TreeSearcher::NAcc::get(TreeSearcher& p, std::unordered_set<size_t>& result, size_t offset) {
		if (p.acc.search().size() == offset) {
			if (p.logic == Logic::EQUAL) {
				NodeMap.leaves.AddToSTLSet(result);
			}
			else {
				NodeMap.get(p, result);//直接调用原始的这个，避免中间层开销
			}
		}
		else {
			auto it = NodeMap.children->find(p.acc.searchU32FourCC(offset));
			if (it != NodeMap.children->end()) {
				it->second->get(p, result, offset + 1);
			}
			for (const auto& [k, v] : index_node) {
				if (!k.match(p.acc.search(), offset, true).empty()) {
					std::unordered_map<uint32_t, std::unique_ptr<Node>>& map = *NodeMap.children;
					for (const auto& c : v) {
						IndexSet::IndexSetIterObj it = p.acc.get(c, offset).GetIterObj();
						for (uint32_t j = it.Next(); j != IndexSetIterEnd; j = it.Next()) {
							map[c]->get(p, result, offset + j);
						}
					}
				}
			}
		}
	}

	void TreeSearcher::NAcc::index(TreeSearcher& p, const uint32_t c) {
		PinIn::Character* ch = p.context->GetCharCachePtr(c);
		if (ch == nullptr) {
			PinIn::Character ch = p.context->GetChar(c);
			for (const auto& py : ch.GetPinyins()) {
				const PinIn::Phoneme& ph = py.GetPhonemes()[0];
				auto it = index_node.find(ph);
				if (it == index_node.end()) {//对应的是字符集合为空
					index_node.insert_or_assign(ph, std::unordered_set<uint32_t>{c});//把汉字插进去
				}
				else {//不为空
					it->second.insert(c);
				}
			}
		}
		else {
			for (const auto& py : ch->GetPinyins()) {
				const PinIn::Phoneme& ph = py.GetPhonemes()[0];
				auto it = index_node.find(ph);
				if (it == index_node.end()) {//对应的是字符集合为空
					index_node.insert_or_assign(ph, std::unordered_set<uint32_t>{c});//把汉字插进去
				}
				else {//不为空
					it->second.insert(c);
				}
			}
		}
	}

	TreeSearcher::Node* TreeSearcher::NSlice::put(TreeSearcher& p, size_t keyword, size_t id) {
		size_t length = end - start;
		size_t match = p.acc.common(start, keyword, length);
		Node* n;
		if (match >= length) {
			n = exit_node->put(p, keyword + length, id);
		}
		else {
			cut(p, start + match);
			n = exit_node->put(p, keyword + match, id);
		}
		if (exit_node.get() != n) {
			p.NodeOwnershipReset(exit_node, n);
		}
		return start == end ? exit_node.release() : this;
	}

	void TreeSearcher::NSlice::cut(TreeSearcher& p, size_t offset) {
		std::unique_ptr<NMap> insert = p.NMapPool.NewObj();//保证异常安全
		if (offset + 1 == end) {//当前exit_node的所有权都会被转移
			insert->put(p.strs.getcharFourCC(offset), std::move(exit_node));
		}
		else {
			std::unique_ptr<NSlice> half = p.NSlicePool.NewObj(p, offset + 1, end);
			half->exit_node = std::move(exit_node);
			insert->put(p.strs.getcharFourCC(offset), std::move(half));
		}
		exit_node = std::move(insert);
		end = offset;
	}

	void TreeSearcher::NSlice::get(TreeSearcher& p, std::unordered_set<size_t>& ret, size_t offset, size_t start) {
		if (this->start + start == end) {
			exit_node->get(p, ret, offset);
		}
		else if (offset == p.acc.search().size()) {
			if (p.logic != Logic::EQUAL) {
				exit_node->get(p, ret);
			}
		}
		else {
			uint32_t ch = p.strs.getcharFourCC(this->start + start);
			IndexSet::IndexSetIterObj it = p.acc.get(ch, offset).GetIterObj();
			for (uint32_t i = it.Next(); i != IndexSetIterEnd; i = it.Next()) {
				get(p, ret, offset + i, start + 1);
			}
		}
	}
}
