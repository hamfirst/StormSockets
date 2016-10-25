
#pragma once

#include "StormGenIndex.h"

namespace StormSockets
{
	struct StormSocketConnectionId
	{
		StormGenIndex m_Index;

		StormSocketConnectionId();
		StormSocketConnectionId(int index, int gen);

		operator int() const;

		static StormSocketConnectionId InvalidConnectionId;

		int GetIndex() const { return m_Index.GetIndex(); }
		int GetGen() const { return m_Index.GetGen(); }
	};
}

namespace std
{
  template< class Key >
  struct hash;

  template <>
  struct hash<StormSockets::StormSocketConnectionId>
  {
    int operator()(StormSockets::StormSocketConnectionId const & id) const
    {
      return (int)id;
    }
  };   
}
