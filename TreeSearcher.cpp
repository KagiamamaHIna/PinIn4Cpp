#include "TreeSearcher.h"

namespace PinInCpp {

	void TreeSearcher::put(const std::string& keyword) {
		size_t pos = strs->put(keyword);
		size_t end = logic == Logic::CONTAIN ? strs->getLastStrSize() : 1;

		for (size_t i = 0; i < end; i++) {
			Node* result = root->put(*this, pos + i, pos);
			if (root.get() != result) {
				root.reset(result);
			}
		}
	}

	TreeSearcher::Node* TreeSearcher::NDense::put(TreeSearcher& p, size_t keyword, size_t id) {
		if (data.size() >= TreeSearcher::NDenseThreshold) {
			size_t pattern = data[0];
			Node* result = new NSlice(pattern, pattern + match(p));
			for (size_t j = 0; j < data.size() / 2; j++) {
				result->put(p, data[j * 2], data[j * 2 + 1]);
			}
			result->put(p, keyword, id);
			return result;
		}
		else {
			data.push_back(keyword);
			data.push_back(id);
			return this;
		}
	}

	size_t TreeSearcher::NDense::match(TreeSearcher& p) {
		for (size_t i = 0; ; i++) {
			if (p.strs->end(data[0] + i)) {//空检查置前，避免额外的字符串构造和std::string比较。而且end实际上比较的是字节，所以速度会更快
				return i;
			}
			std::string a = p.strs->getchar(data[0] + i);
			for (size_t j = 1; j < data.size() / 2; j++) {
				std::string b = p.strs->getchar(data[j * 2] + i);
				if (a != b) return i;//空检查置前了，所以这里可以删除空检查
			}
		}
	}
}
