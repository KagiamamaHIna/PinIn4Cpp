#include "PinIn4Cpp/detail/IndexSet.h"

namespace PinInCpp {
	namespace detail {
		const IndexSet IndexSet::ZERO = IndexSet::Init(1);
		const IndexSet IndexSet::ONE = IndexSet::Init(2);
		const IndexSet IndexSet::NONE = IndexSet::Init(0);
	}
}
