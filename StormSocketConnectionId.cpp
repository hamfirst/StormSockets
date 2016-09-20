
#include "StormSocketConnectionId.h"

namespace StormSockets
{
	StormSocketConnectionId StormSocketConnectionId::InvalidConnectionId = StormSocketConnectionId(-1, 0);

	StormSocketConnectionId::StormSocketConnectionId()
	{
		m_Index.Raw = 0;
	}

	StormSocketConnectionId::StormSocketConnectionId(int index, int gen)
	{
		m_Index = StormGenIndex(index, gen);
	}

	StormSocketConnectionId::operator int() const
	{
		return m_Index.GetIndex();
	}
}
