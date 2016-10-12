
#include "StormUrlUtil.h"

namespace StormSockets
{
  bool ValidSchemeCharacter(char c)
  {
    if (c >= 'a' && c <= 'z')
    {
      return true;
    }

    if (c >= 'A' && c <= 'Z')
    {
      return true;
    }

    if (c >= '0' && c <= '9')
    {
      return true;
    }

    if (c == '+' || c == '.' || c == '-')
    {
      return true;
    }

    return false;
  }

  bool ValidHostCharacter(char c)
  {
    if (c >= 'a' && c <= 'z')
    {
      return true;
    }

    if (c >= 'A' && c <= 'Z')
    {
      return true;
    }

    if (c >= '0' && c <= '9')
    {
      return true;
    }

    if (c == '.' || c == '-')
    {
      return true;
    }

    return false;
  }


  bool ParseURI(const char * uri, StormURI & out_uri)
  {
    if (*uri == ':')
    {
      return false;
    }

    while (*uri != ':')
    {
      if (ValidSchemeCharacter(*uri) == false)
      {
        return false;
      }

      out_uri.m_Protocol.push_back(*uri);
      uri++;
    }

    uri++;
    if (*uri != '/')
    {
      return false;
    }

    uri++;
    if (*uri != '/')
    {
      return false;
    }

    uri++;
    if (*uri == '/' || *uri == ':')
    {
      return false;
    }

    while (*uri != '/' && *uri != ':')
    {
      if (ValidHostCharacter(*uri) == false)
      {
        return false;
      }

      out_uri.m_Host.push_back(*uri);
      uri++;
    }

    if (*uri == ':')
    {
      uri++;
      if (*uri == '/')
      {
        return false;
      }

      while (*uri != '/')
      {
        if (*uri < '0' || *uri > '9')
        {
          return false;
        }

        out_uri.m_Port.push_back(*uri);
        uri++;
      }
    }

    out_uri.m_Uri.push_back(*uri);
    uri++;

    while (*uri != 0 && *uri != '#')
    {
      out_uri.m_Uri.push_back(*uri);
      uri++;
    }

    return true;
  }
}