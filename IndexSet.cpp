#include "IndexSet.h"

namespace PinInCpp {
	const IndexSet IndexSet::ZERO = IndexSet::Init(1);
	const IndexSet IndexSet::ONE = IndexSet::Init(2);
	const IndexSet IndexSet::NONE = IndexSet::Init();

	bool IndexSet::traverse(IntPredicate p)const {
		uint32_t v = value;
		for (uint32_t i = 0; i < 32; i++) {
			if ((v & 0x1) == 0x1 && p(i)) {
				return true;
			}
			else if (v == 0) {
				return false;
			}
			v >>= 1;
		}
		return false;
	}
	void IndexSet::foreach(IntConsumer c)const {
		uint32_t v = value;
		for (uint32_t i = 0; i < 32; i++) {
			if ((v & 0x1) == 0x1) {
				c(i);
			}
			else if (v == 0) {
				return;
			}
			v >>= 1;
		}
	}
	bool operator==(IndexSet a, IndexSet b) {
		return a.value == b.value;
	}
}
