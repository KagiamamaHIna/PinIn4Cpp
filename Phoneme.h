#pragma once
#include <vector>
#include <string>
#include <set>

#include "PinIn.h"

namespace PinInCpp {
	class Phoneme {
	public:

		void reload(const std::string& str, const PinIn& p) {
			strs = {};
			strs.push_back(str);

		}
	private:
		std::vector<std::string> strs;
	};
}
