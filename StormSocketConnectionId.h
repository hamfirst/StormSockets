
#pragma once

#include "StormGenIndex.h"

#include <functional>

namespace StormSockets
{
	struct StormSocketConnectionId
	{
		StormGenIndex m_Index;

		StormSocketConnectionId();
		StormSocketConnectionId(int index, int gen);

    StormSocketConnectionId(const StormSocketConnectionId & rhs) = default;
    StormSocketConnectionId(StormSocketConnectionId && rhs) = default;

    StormSocketConnectionId & operator =(const StormSocketConnectionId & rhs) = default;
    StormSocketConnectionId & operator =(StormSocketConnectionId && rhs) = default;

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
      return (int)id.m_Index.Raw;
    }
  };   
}
