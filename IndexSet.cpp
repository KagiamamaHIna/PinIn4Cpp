#include "IndexSet.h"

namespace PinInCpp {
	const IndexSet IndexSet::ZERO(1);
	const IndexSet IndexSet::ONE(2);
	const IndexSet IndexSet::NONE(0);

	bool IndexSet::traverse(IntPredicate p)const {
		uint32_t v = value;
		for (int i = 0; i < 7; i++) {
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
		for (int i = 0; i < 7; i++) {
			if ((v & 0x1) == 0x1) {
				c(i);
			}
			else if (v == 0) {
				return;
			}
			v >>= 1;
		}
	}
}
