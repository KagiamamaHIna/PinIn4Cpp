#include "TreeSearcher.h"

namespace PinInCpp {
	void TreeSearcher::put(std::string_view keyword) {
		ticket->renew();
		size_t pos = strs.put(keyword);
		size_t end = logic == Logic::CONTAIN ? strs.getLastOffset() - 1 : pos + 1;
		Node* result = root->putRange(*this, pos, end);
		if (root.get() != result) {
			NodeOwnershipReset(root, result);
		}
	}

	std::vector<std::string> TreeSearcher::ExecuteSearch(std::string_view s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);

		std::vector<std::string> result;
		result.reserve(ret.size());
		for (const size_t id : ret) {//基本类型复制更高效
			result.emplace_back(strs.getstr(id));
		}
		return result;
	}

	std::unordered_set<size_t> TreeSearcher::ExecuteSearchGetSet(std::string_view s) {
		std::unordered_set<size_t> ret;
		CommonSearch(s, ret);
		return ret;
	}

	std::unique_ptr<TreeSearcher::Node> TreeSearcher::Node::Deserialize(TreeSearcher& p, VecU8Reader& reader) {
		NodeType type = static_cast<NodeType>(reader.GetByte());

		std::unique_ptr<Node> result;
		switch (type) {
		case NodeType::NDenseType: {
			result = NDense::Deserialize(reader);
			break;
		}
		case NodeType::NSliceType: {
			result = NSlice::Deserialize(p, reader);
			break;
		}
		case NodeType::NMapType: {
			result = NMap::Deserialize(p, reader);
			break;
		}
		case NodeType::NAccType: {
			result = NAcc::Deserialize(p, reader);
			break;
		}
		}

		return result;
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

	TreeSearcher::Node* TreeSearcher::NDense::putRange(TreeSearcher& p, size_t start, size_t end) {
		size_t NewSize = data.size() + end - start;
		if (NewSize >= TreeSearcher::NDenseThreshold) {
			size_t pattern = data[0];
			std::unique_ptr<Node> result = p.NSlicePool.NewObj(p, pattern, pattern + match(p));
			Node* other = result.get();
			for (size_t j = 0; j < data.size(); j += 2) {//数据转移
				other = result->put(p, data[j], data[j + 1]);
				if (other != result.get()) {//节点升级
					p.NodeOwnershipReset(result, other);
					//因为other本质上已经是新指针了，所以不用再次赋值。result通过这个转移所有权的函数现在他和other是同一个指针
				}
			}
			other = result->putRange(p, start, end);
			if (other != result.get()) {//节点升级
				p.NodeOwnershipReset(result, other);
			}
			return result.release();
		}
		else {
			for (size_t i = start; i < end; i++) {
				data.emplace_back(i);
				data.emplace_back(start);
			}
			return this;
		}
	}

	void TreeSearcher::NDense::Serialize(std::vector<uint8_t>& data)const {
		data.push_back(static_cast<uint8_t>(NodeType::NDenseType));
		PushQWUint8(data, this->data.size());
		for (const auto& v : this->data) {
			PushQWUint8(data, v);
		}
	}

	std::unique_ptr<TreeSearcher::NDense> TreeSearcher::NDense::Deserialize(VecU8Reader& reader) {
		size_t dataSize = reader.GetSizeTFromQW();

		std::unique_ptr<NDense> result = std::make_unique<NDense>();

		for (size_t i = 0; i < dataSize; i++) {
			result->data.emplace_back(reader.GetSizeTFromQW());
		}

		return result;
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
			if (it != nullptr) {
				it->get()->get(p, result, offset + 1);
			}
			for (const auto& [k, v] : index_node) {
				if (!k->match(p.acc.search(), offset, true).empty()) {
					AdaptiveMap<uint32_t, std::unique_ptr<Node>>& map = *NodeMap.children;
					v.forEach([&](uint32_t c) {
						IndexSet::IndexSetIterObj it = p.acc.get(c, offset).GetIterObj();
						for (uint32_t j = it.Next(); j != it.end(); j = it.Next()) {
							map[c]->get(p, result, offset + j);
						}
					});
				}
			}
		}
	}

	TreeSearcher::Node* TreeSearcher::NAcc::put(TreeSearcher& p, size_t keyword, size_t id) {
		NodeMap.put(p, keyword, id);//绝对不会升级，不需要检查
		if (p.context->IsCharCacheEnabled()) {
			indexUseCache(p, p.strs.getcharFourCC(keyword));//put完后构建索引，并且不再有put操作，应该是安全的
		}
		else {
			indexNotUseCache(p, p.strs.getcharFourCC(keyword));
		}
		return this;
	}

	void TreeSearcher::NAcc::reload(TreeSearcher& p) {
		index_node.clear();//释放所有音素
		if (p.context->IsCharCacheEnabled()) {
			NodeMap.children->forEach([&](const auto& k, const auto& v) {
				indexUseCache(p, k);
			});
		}
		else {
			NodeMap.children->forEach([&](const auto& k, const auto& v) {
				indexNotUseCache(p, k);
			});
		}
	}

	TreeSearcher::Node* TreeSearcher::NAcc::putRange(TreeSearcher& p, size_t start, size_t end) {
		NodeMap.putRange(p, start, end);
		if (p.context->IsCharCacheEnabled()) {
			for (size_t i = start; i < end; i++) {
				indexUseCache(p, p.strs.getcharFourCC(i));//put完后构建索引，并且不再有put操作，应该是安全的
			}
		}
		else {
			for (size_t i = start; i < end; i++) {
				indexNotUseCache(p, p.strs.getcharFourCC(i));//put完后构建索引，并且不再有put操作，应该是安全的
			}
		}
		return this;
	}

	void TreeSearcher::NAcc::indexUseCache(TreeSearcher& p, const uint32_t c) {
		PinIn::Character* ch = p.context->GetCharCachePtr(c);
		for (const auto& py : ch->GetPinyins()) {
			PinIn::Phoneme* ph = py->GetPhonemes()[0];
			auto it = index_node.find(ph);
			if (it == index_node.end()) {//对应的是字符集合为空
				index_node.insert_or_assign(ph, AdaptiveSet<uint32_t>{c});//把汉字插进去
			}
			else {//不为空
				it->second.insert(c);
			}
		}
	}

	void TreeSearcher::NAcc::indexNotUseCache(TreeSearcher& p, const uint32_t c) {
		PinIn::Character ch = p.context->GetChar(c);
		for (const auto& py : ch.GetPinyins()) {
			PinIn::Phoneme* ph = py->GetPhonemes()[0];
			auto it = index_node.find(ph);
			if (it == index_node.end()) {//对应的是字符集合为空
				index_node.insert_or_assign(ph, AdaptiveSet<uint32_t>{c});//把汉字插进去
			}
			else {//不为空
				it->second.insert(c);
			}
		}
	}

	void TreeSearcher::NAcc::Serialize(std::vector<uint8_t>& data)const {
		data.push_back(static_cast<uint8_t>(NodeType::NAccType));
		NodeMap.Serialize(data);
	}

	std::unique_ptr<TreeSearcher::NAcc> TreeSearcher::NAcc::Deserialize(TreeSearcher& p, VecU8Reader& reader) {
		std::unique_ptr<NMap> temp = NMap::Deserialize<true>(p, reader);
		std::unique_ptr<NAcc> result = std::make_unique<NAcc>(p, *temp);
		temp->FreeToPool(p);//释放到池里面，方便下一次的对象复用
		temp.release();//解除所有权，避免被delete了

		return result;
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

	TreeSearcher::Node* TreeSearcher::NSlice::putRange(TreeSearcher& p, size_t start, size_t end) {
		Node* n;
		for (size_t i = start; i < end; i++) {
			size_t length = this->end - this->start;
			size_t match = p.acc.common(this->start, i, length);
			if (match >= length) {
				n = exit_node->put(p, i + length, start);
			}
			else {
				cut(p, this->start + match);
				n = exit_node->put(p, i + match, start);
			}
			if (exit_node.get() != n) {
				p.NodeOwnershipReset(exit_node, n);
			}
		}
		return start == end ? exit_node.release() : this;
	}

	void TreeSearcher::NSlice::Serialize(std::vector<uint8_t>& data)const {
		data.push_back(static_cast<uint8_t>(NodeType::NSliceType));
		PushQWUint8(data, start);
		PushQWUint8(data, end);
		exit_node->Serialize(data);
	}

	std::unique_ptr<TreeSearcher::NSlice> TreeSearcher::NSlice::Deserialize(TreeSearcher& p, VecU8Reader& reader) {
		size_t start = reader.GetSizeTFromQW();
		size_t end = reader.GetSizeTFromQW();
		std::unique_ptr<NSlice> result = std::unique_ptr<NSlice>(new NSlice(start, end));
		result->exit_node = Node::Deserialize(p, reader);

		return result;
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
			for (uint32_t i = it.Next(); i != it.end(); i = it.Next()) {
				get(p, ret, offset + i, start + 1);
			}
		}
	}

	std::unique_ptr<TreeSearcher> TreeSearcher::Deserialize(const std::vector<uint8_t>& data, std::shared_ptr<PinIn> PinInShared, size_t index) {
		VecU8Reader reader(data, index);

		uint32_t ver = reader.GetDoubleWord();
		if (ver != BinDataVersion) {
			throw BinaryVersionInvalidException("TreeSearcher: Invalid binary file version");
		}
		Logic logic = static_cast<Logic>(reader.GetByte());

		std::unique_ptr<TreeSearcher> result = std::make_unique<TreeSearcher>(logic, PinInShared);

		size_t StrPoolSize = reader.GetSizeTFromQW();

		result->strs = UTF8StringPool::Deserialize(reader.GetData(), reader.GetIndex());
		reader.AddIndex(StrPoolSize);

		result->root = Node::Deserialize(*result, reader);
		return result;
	}

	std::vector<uint8_t> TreeSearcher::Serialize()const {
		std::vector<uint8_t> result;
		PushDWUint8(result, BinDataVersion);
		result.push_back(static_cast<uint8_t>(logic));

		std::vector<uint8_t> StrPoolData = strs.Serialize();//字符串池数据
		PushQWUint8(result, StrPoolData.size());
		result.insert(result.end(), StrPoolData.begin(), StrPoolData.end());

		root->Serialize(result);
		return result;
	}
}
